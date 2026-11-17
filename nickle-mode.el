;;; nickle-mode.el --- Major mode for Nickle source files  -*- lexical-binding: t; -*-

;; Copyright © 2026 Keith Packard and Bart Massey.
;; All Rights Reserved.  See the file COPYING in this directory
;; for licensing information.

;; Author: Keith Packard <keithp@keithp.com>
;; Version: 1.0
;; Package-Requires: ((emacs "26.1"))
;; Keywords: languages
;; URL: https://nickle.org

;;; Commentary:

;; A major mode for Nickle (.5c) files, indenting them as
;; nickle --format lays them out: four columns a level, a switch's case
;; labels at the switch's level, a function's braces at its header's,
;; and lines continuing a statement four columns in, or lined up after
;; an opening parenthesis, bracket, or brace with text after it.
;;
;; Nickle comments are /* ... */, and lines whose first character
;; other than blanks is '#'.  Elsewhere '#' is the base operator, as in
;; x # 16, and '//' is integer division, not a comment, so C mode
;; misreads Nickle.  Comment commands write /* */, as a '#' comment
;; can't follow code on its line.
;;
;; M-x nickle-format-buffer (C-c C-f) reformats the buffer with
;; nickle --pretty.  M-x run-nickle (C-c C-z) runs Nickle in a buffer;
;; C-c C-r and C-c C-b send it the region or the buffer, and C-c C-l
;; has it load the buffer's file.  M-q fills a comment's paragraph,
;; keeping its ' * ' or '#' marks.

;;; Code:

(require 'comint)

(defgroup nickle nil
  "Editing Nickle source files."
  :group 'languages
  :prefix "nickle-")

(defcustom nickle-indent-offset 4
  "Columns each level of Nickle code is indented."
  :type 'integer
  :safe #'integerp)

(defcustom nickle-program "nickle"
  "The nickle program `nickle-format-buffer' runs."
  :type 'string)

(defconst nickle-keywords
  '("auto" "break" "case" "catch" "const" "continue" "default" "do" "else"
    "enum" "exception" "extend" "for" "fork" "func" "function" "global"
    "has_member" "if" "import" "is_type" "namespace" "protected" "public"
    "raise" "return" "static" "struct" "switch" "try" "twixt" "typedef"
    "union" "while")
  "Nickle's keywords, from lex.l.")

(defconst nickle-types
  '("bool" "complex" "continuation" "file" "foreign" "int" "noreturn" "poly"
    "rational" "real" "semaphore" "string" "thread" "void")
  "Nickle's built-in type names, from lex.l.")

(defconst nickle-constants '("true" "false")
  "Nickle's built-in constants.")

(defvar nickle-mode-syntax-table
  (let ((table (make-syntax-table)))
    ;; /* ... */ comments; '#' comments are marked by
    ;; `nickle-syntax-propertize', as only a '#' starting a line is one
    (modify-syntax-entry ?/ ". 14" table)
    (modify-syntax-entry ?* ". 23" table)
    (modify-syntax-entry ?\n "> b" table)
    (modify-syntax-entry ?# "." table)
    (modify-syntax-entry ?_ "_" table)
    (modify-syntax-entry ?\' "\"" table)
    (modify-syntax-entry ?\" "\"" table)
    (modify-syntax-entry ?\\ "\\" table)
    (dolist (c '(?+ ?- ?% ?< ?> ?& ?| ?^ ?! ?= ?~ ?? ?: ?.))
      (modify-syntax-entry c "." table))
    table)
  "Syntax table for `nickle-mode'.")

(defconst nickle-syntax-propertize
  (syntax-propertize-rules ("^[ \t]*\\(#\\)" (1 "< b")))
  "Mark a '#' starting a line as starting a comment.")

(defconst nickle-font-lock-keywords
  `((,(regexp-opt nickle-keywords 'symbols) . font-lock-keyword-face)
    (,(regexp-opt nickle-types 'symbols) . font-lock-type-face)
    (,(regexp-opt nickle-constants 'symbols) . font-lock-constant-face)
    ;; A definition's name starts its line, as nickle --format puts it
    ("^\\(\\(?:\\sw\\|\\s_\\)+\\)\\s-*(" 1 font-lock-function-name-face))
  "Highlighting for `nickle-mode'.")

;;; Indentation

(defun nickle--code-line-p ()
  "Whether the current line holds code, not just blanks or comments."
  (save-excursion
    (back-to-indentation)
    (let ((in (save-excursion (syntax-ppss (point)))))
      ;; A comment from lines above may end here, before code
      (when (and (nth 4 in) (not (nth 7 in)))
        (goto-char (nth 8 in))
        (forward-comment 1)
        (skip-chars-forward " \t")))
    (unless (or (eolp) (< (point) (line-beginning-position))
                (nth 4 (save-excursion (syntax-ppss (point)))) (looking-at-p "#"))
      (let ((eol (line-end-position)))
        ;; Comments before code on the line, as in /* doc */;
        (while (and (looking-at-p "/\\*") (forward-comment 1) (<= (point) eol))
          (skip-chars-forward " \t"))
        (and (< (point) eol) (not (looking-at-p "/\\*")))))))

(defun nickle--previous-code-line ()
  "Move to the last line before this one holding code; nil if none."
  (let (found)
    (while (and (not found) (zerop (forward-line -1)))
      (when (nickle--code-line-p)
        (setq found t)))
    found))

(defun nickle--line-end-pos ()
  "Where the current line's code ends, ignoring comments; nil if it has none."
  (save-excursion
    (end-of-line)
    (let ((bol (line-beginning-position)))
      (forward-comment (- (point)))
      (while (and (> (point) bol) (nth 4 (save-excursion (syntax-ppss (point)))))
        (goto-char (nth 8 (save-excursion (syntax-ppss (point)))))
        (skip-chars-backward " \t"))
      (and (> (point) bol) (point)))))

(defun nickle--line-end-char ()
  "The last character of the current line's code, ignoring comments."
  (let ((end (nickle--line-end-pos)))
    (and end (char-before end))))

(defconst nickle--header-regexp
  "\\(?:}[ \t]*\\)?\\(?:else[ \t]+\\)?\\_<\\(?:if\\|while\\|for\\|twixt\\|switch\\)\\_>"
  "A control statement's header, which may start with '} else'.")

(defun nickle--header-indentation ()
  "If the current line ends a control header without a brace, its indentation.
A header's parenthesized part may span lines; else and do are
headers alone."
  (save-excursion
    (let ((end (nickle--line-end-pos)))
      (when end
        (goto-char end)
        (cond
         ((eq (char-before) ?\))
          (condition-case nil
              (progn
                (backward-sexp)
                (back-to-indentation)
                (and (looking-at-p nickle--header-regexp) (current-indentation)))
            (error nil)))
         ((looking-back "\\(?:^\\|}\\)[ \t]*\\_<\\(?:else\\|do\\)" (line-beginning-position))
          (current-indentation)))))))

(defconst nickle--modifiers
  (regexp-opt '("public" "protected" "static" "global" "const" "auto" "function") 'symbols)
  "Words that may precede a definition's return type.")

(defun nickle--type-line-p ()
  "Whether the current line is only a definition's return type."
  (save-excursion
    (back-to-indentation)
    (while (looking-at (concat nickle--modifiers "[ \t]*"))
      (goto-char (match-end 0)))
    (and (looking-at-p "[[:alpha:]_*&]")
         (or (looking-at-p (regexp-opt nickle-types 'symbols))
             (not (looking-at-p (concat (regexp-opt nickle-keywords 'symbols)
                                        "\\|"
                                        (regexp-opt '("true" "false") 'symbols)))))
         (not (string-match-p (if (looking-at-p (regexp-opt nickle-types 'symbols))
                                  "[;={}]"
                                "[;=(){}]")
                              (buffer-substring (point) (line-end-position)))))))

(defun nickle--switch-brace-p (pos)
  "Whether the brace at POS opens a switch's body."
  (save-excursion
    (goto-char pos)
    (skip-chars-backward " \t\n")
    (and (eq (char-before) ?\))
         (condition-case nil
             (progn (backward-sexp) (skip-chars-backward " \t")
                    (looking-back "\\_<switch" (line-beginning-position)))
           (error nil)))))

(defun nickle--brace-column (pos)
  "The column the block opened by the brace at POS is laid out from.
A lambda's block is laid out from where the lambda starts, as in
check(bool func() {; any other block from its line's indentation."
  (save-excursion
    (goto-char pos)
    (let ((bol (line-beginning-position)))
      (if (re-search-backward "\\_<func\\_>" bol t)
          (let ((func (point)))
            ;; A lambda's return type comes before 'func', as in bool func()
            (skip-chars-backward " \t")
            (if (and (> (point) bol)
                     (looking-back "\\(?:\\sw\\|\\s_\\|[]*&[]\\)+" bol t)
                     (progn (goto-char (match-beginning 0))
                            (not (looking-at-p
                                  (concat (regexp-opt '("fork" "return" "raise") 'symbols))))))
                (current-column)
              (goto-char func)
              (current-column)))
        (current-indentation)))))

(defun nickle--after-opener (pos)
  "The column of the text after the bracket at POS on its line, or nil."
  (save-excursion
    (goto-char (1+ pos))
    (skip-chars-forward " \t")
    (unless (or (eolp) (looking-at-p "/\\*\\|#"))
      (current-column))))

(defun nickle--continues-p (char)
  "Whether a line ending with CHAR leaves its statement unfinished."
  (and char (not (memq char '(?\; ?{ ?} ?: ?,)))))

(defun nickle--line-open ()
  "The innermost bracket open where the current line starts."
  (nth 1 (save-excursion (syntax-ppss (line-beginning-position)))))

(defun nickle--paren-indentation (open)
  "Indentation inside the parenthesis or bracket at OPEN.
It follows the code line before, inside the same bracket."
  (save-excursion
    (let ((opener-line (save-excursion (goto-char open) (line-beginning-position))))
      (if (and (nickle--previous-code-line)
               (> (point) opener-line)
               (eq (nickle--line-open) open))
          ;; A line inside the brackets before this one lines this one up
          (current-indentation)
        (goto-char open)
        (let ((after (nickle--after-opener open)))
          (cond
           ;; A value moved past its '=' starts four columns in
           ((and after (eq (nickle--line-end-char) ?=))
            (+ (current-indentation) nickle-indent-offset))
           ;; A broken header of a body without braces goes four further
           ((and after (nickle--braceless-header-p open))
            (+ after nickle-indent-offset))
           (after after)
           (t (+ (current-indentation) nickle-indent-offset))))))))

(defun nickle--braceless-header-p (open)
  "Whether the parenthesis at OPEN holds a control header whose body has no brace."
  (save-excursion
    (goto-char open)
    (and (looking-back "\\_<\\(?:if\\|while\\)[ \t]*" (line-beginning-position))
         (condition-case nil
             (progn (forward-sexp)
                    ;; A broken header's brace starts the next line
                    (forward-comment (buffer-size))
                    (not (looking-at-p "{")))
           (error nil)))))

(defun nickle-calculate-indentation ()
  "The column the current line's code belongs at."
  (save-excursion
    (back-to-indentation)
    (let* ((open (nickle--line-open))
           (open-char (and open (char-after open)))
           (closing (looking-at-p "[]})]"))
           (label (looking-at-p "\\_<\\(?:case\\_>\\|default[ \t]*:\\)"))
           (brace (looking-at-p "{"))
           (name (looking-at-p "\\(?:\\sw\\|\\s_\\)+[ \t]*(")))
      (cond
       ;; A closing bracket goes under its opener's line or lambda
       (closing
        (cond ((null open) 0)
              ((eq open-char ?{) (nickle--brace-column open))
              (t (save-excursion (goto-char open) (current-indentation)))))
       ((and open (not (eq open-char ?{)))
        (nickle--paren-indentation open))
       (t
        (let* ((after (and open (nickle--after-opener open)))
               (switch (and open (nickle--switch-brace-p open)))
               (base (cond ((null open) 0)
                           (after after)
                           (t (+ (nickle--brace-column open) nickle-indent-offset)))))
          (cond
           ((and switch (or label (and (looking-at-p "/\\*\\|#") (nickle--next-label-p))))
            (- base nickle-indent-offset))
           (label base)
           (t
            (save-excursion
              (let ((comment (looking-at-p "/\\*\\|#"))
                    (else (looking-at-p "\\_<else\\_>"))
                    (while-do (looking-at-p "\\_<while\\_>.*;[ \t]*$")))
                (if (and (nickle--previous-code-line)
                         (or (null open) (> (point) open)))
                    (let ((header (and (not brace) (nickle--header-indentation)))
                          (prev (current-indentation)))
                      (cond
                       ;; An else goes with its if, a do's while with its do
                       ((and else (nickle--matching-indentation
                                   "\\_<if\\_>\\|\\_<else[ \t]+if\\_>" "\\_<else\\_>" prev)))
                       ((and while-do (nickle--matching-indentation
                                       "\\_<do\\_>" "\\_<while\\_>.*;[ \t]*$" prev)))
                       (header (+ header nickle-indent-offset))
                       ((and name (nickle--type-line-p)) (current-indentation))
                       ;; A value moved past its '=', a block or not
                       ((and open (eq (nickle--line-end-char) ?=))
                        (+ (nickle--statement-line-indentation open) nickle-indent-offset))
                       (brace base)
                       ;; A top-level definition's doc comment goes under its header
                       ((and (null open) comment (eq (nickle--line-end-char) ?\))
                             (nickle--definition-header-p))
                        (+ (nickle--statement-line-indentation open) nickle-indent-offset))
                       ;; Outside braces, a newline ends a command
                       ((null open) base)
                       ((nickle--continues-p (nickle--line-end-char))
                        (if (nickle--continuation-p open)
                            (current-indentation)
                          (+ (nickle--statement-line-indentation open) nickle-indent-offset)))
                       (t base)))
                  base)))))))))))

(defun nickle--statement-line-indentation (open)
  "Indentation of the line starting the statement the current line ends.
A statement whose parentheses span lines starts where they open; OPEN
is the brace the statement is in."
  (save-excursion
    (let ((o (nickle--line-open)))
      (while (and o (not (eq o open)) (or (null open) (> o open)))
        (goto-char o)
        (setq o (nickle--line-open))))
    (current-indentation)))

(defun nickle--matching-indentation (opener closer prev)
  "Indentation of the OPENER line a CLOSER line pairs with, as an else
with its if; PREV is the indentation of the line before it."
  (save-excursion
    (let ((used nil) (found nil))
      (while (and (not found) (nickle--previous-code-line))
        (let ((col (current-indentation)))
          (back-to-indentation)
          ;; An else if both pairs with an if and opens the next pair
          (cond
           ((and (looking-at-p opener) (< col prev))
            (if (memq col used)
                (setq used (delete col used))
              (setq found col)))
           ((looking-at-p closer) (push col used)))))
      found)))

(defun nickle--definition-header-p ()
  "Whether the current line ends a definition's header: a name and
parameters under its return type."
  (save-excursion
    (goto-char (nickle--line-end-pos))
    (condition-case nil
        (progn (backward-sexp) (back-to-indentation)
               (and (looking-at-p "\\(?:\\sw\\|\\s_\\)+[ \t]*(")
                    (nickle--previous-code-line)
                    (nickle--type-line-p)))
      (error nil))))

(defun nickle--next-label-p ()
  "Whether the next line with code is a case label or the closing brace."
  (save-excursion
    (let (found)
      (while (and (not found) (zerop (forward-line 1)) (not (eobp)))
        (when (nickle--code-line-p) (setq found t)))
      (and found (progn (back-to-indentation)
                        (looking-at-p "}\\|\\_<\\(?:case\\_>\\|default[ \t]*:\\)"))))))

(defun nickle--continuation-p (open)
  "Whether the current line continues a statement from the line before it.
OPEN is the brace the lines are in."
  (save-excursion
    (and (nickle--previous-code-line)
         (> (point) open)
         (not (nickle--header-indentation))
         (not (nickle--type-line-p))
         (nickle--continues-p (nickle--line-end-char)))))

(defun nickle-indent-line ()
  "Indent the current line as nickle --format lays it out."
  (interactive)
  (let ((ppss (save-excursion (syntax-ppss (line-beginning-position)))))
    (unless (or (nth 3 ppss) (nth 4 ppss))
      (let ((col (nickle-calculate-indentation))
            (offset (- (current-column) (current-indentation))))
        (indent-line-to (max col 0))
        (when (> offset 0)
          (forward-char offset))))))

;;; Filling comments

(defun nickle--comment-lines (start end prefix-re)
  "The bounds of the paragraph at point among lines of a comment.
START and END bound the lines it may take; a line holding only what
PREFIX-RE matches separates paragraphs.  Return (BEG . END), or nil
when point is on such a line."
  (let ((blank (concat prefix-re "[ \t]*$")))
    (save-excursion
      (beginning-of-line)
      (unless (looking-at-p blank)
        (let ((beg (point)) (fin (line-end-position)))
          (while (and (zerop (forward-line -1)) (>= (point) start)
                      (not (looking-at-p blank)))
            (setq beg (point)))
          (goto-char fin)
          (while (and (zerop (forward-line 1)) (< (line-end-position) end)
                      (not (looking-at-p blank)))
            (setq fin (line-end-position)))
          (cons beg fin))))))

(defun nickle--fill-lines (bounds prefix justify)
  "Fill the lines in BOUNDS as one paragraph, each starting with PREFIX.
JUSTIFY is as for `fill-paragraph'."
  (let ((fill-prefix prefix))
    (fill-region-as-paragraph (car bounds) (cdr bounds) justify)))

(defun nickle--fill-hash-comment (justify)
  "Fill the paragraph of the '#' comment at point; JUSTIFY as for `fill-paragraph'."
  (save-excursion
    (beginning-of-line)
    (when (looking-at "[ \t]*#+ ?")
      (let* ((prefix (match-string 0))
             (start (save-excursion
                      (while (and (zerop (forward-line -1)) (looking-at-p "[ \t]*#")))
                      (unless (looking-at-p "[ \t]*#") (forward-line 1))
                      (point)))
             (end (save-excursion
                    (while (and (looking-at-p "[ \t]*#") (zerop (forward-line 1))))
                    (point)))
             (bounds (nickle--comment-lines start end "[ \t]*#+")))
        (when bounds
          (nickle--fill-lines bounds (if (string-match-p " $" prefix) prefix (concat prefix " "))
                              justify))
        t))))

(defun nickle--fill-block-comment (start justify)
  "Fill the paragraph of the /* */ comment at START holding point.
A one-line comment too long for `fill-column' becomes a block with
' * ' starting each line; JUSTIFY is as for `fill-paragraph'."
  (let ((end (save-excursion (goto-char start) (forward-comment 1) (point)))
        (indent (save-excursion (goto-char start) (current-column))))
    (save-excursion
      (goto-char start)
      ;; A comment on one line, or opening with text, becomes a block
      (when (looking-at "/\\*+[ \t]*\\([^ \t\n]\\)")
        (let ((text-start (match-beginning 1)))
          (goto-char end)
          (when (looking-back "[ \t]*\\*+/" start)
            (let ((close (match-beginning 0)))
              (if (and (= (count-lines start end) 1)
                       (<= (save-excursion (goto-char end) (current-column)) fill-column))
                  (setq end nil)
                (delete-region close end)
                (insert "\n" (make-string indent ?\s) " */")
                (goto-char text-start)
                (delete-region (save-excursion (goto-char start) (+ (point) 2)) text-start)
                (insert "\n" (make-string indent ?\s) " * ")
                (setq end (save-excursion (goto-char start) (forward-comment 1) (point)))))))))
    (when end
      (save-excursion
        ;; Lines between the opening and closing lines, with or without ' * '
        (let* ((first (save-excursion (goto-char start) (forward-line 1) (point)))
               (last (save-excursion (goto-char end) (line-beginning-position)))
               (star (save-excursion
                       (goto-char (max (point) first))
                       (beginning-of-line)
                       (looking-at "[ \t]*\\*+ ?")))
               (prefix (if star
                           (let ((p (match-string 0)))
                             (if (string-match-p " $" p) p (concat p " ")))
                         (make-string (+ indent 3) ?\s)))
               (bounds (and (>= (line-beginning-position) first)
                            (< (line-beginning-position) last)
                            (nickle--comment-lines first last
                                                   (if star "[ \t]*\\*+" "[ \t]*")))))
          (when bounds
            (nickle--fill-lines bounds prefix justify)))))
    t))

(defun nickle-fill-paragraph (&optional justify)
  "Fill the comment paragraph at point, keeping the comment's form.
A /* */ comment keeps its ' * ' starting each line, and a '#'
comment its '#' marks; JUSTIFY is as for `fill-paragraph'.  Code
isn't text to fill, so outside comments this does nothing."
  (let* ((ppss (save-excursion (syntax-ppss (point))))
         (start (and (nth 4 ppss) (nth 8 ppss))))
    (cond
     ((save-excursion (beginning-of-line) (looking-at-p "[ \t]*#"))
      (nickle--fill-hash-comment justify))
     (start (nickle--fill-block-comment start justify))
     ((save-excursion (back-to-indentation) (looking-at-p "/\\*"))
      (nickle--fill-block-comment (save-excursion (back-to-indentation) (point)) justify))
     (t t))))

;;; Running Nickle

(defcustom nickle-program-args nil
  "Arguments `run-nickle' passes to `nickle-program'."
  :type '(repeat string))

(defconst nickle-process-buffer "*nickle*"
  "The buffer `run-nickle' runs Nickle in.")

(define-derived-mode inferior-nickle-mode comint-mode "Inferior Nickle"
  "Major mode for interacting with Nickle, as `run-nickle' starts it.

\\{inferior-nickle-mode-map}"
  :syntax-table nickle-mode-syntax-table
  (setq-local comint-prompt-regexp "^[>+] ")
  (setq-local comint-prompt-read-only t)
  (setq-local font-lock-defaults '(nickle-font-lock-keywords)))

(defun nickle--process ()
  "The Nickle process `run-nickle' runs, starting it if needed."
  (let ((buffer (get-buffer-create nickle-process-buffer)))
    (unless (comint-check-proc buffer)
      (apply #'make-comint-in-buffer "Nickle" buffer nickle-program nil nickle-program-args)
      (with-current-buffer buffer (inferior-nickle-mode)))
    (get-buffer-process buffer)))

;;;###autoload
(defun run-nickle ()
  "Run Nickle in a buffer, or switch to the one running."
  (interactive)
  (nickle--process)
  (pop-to-buffer nickle-process-buffer))

(defun nickle-send-region (start end)
  "Send the region from START to END to Nickle, as if typed."
  (interactive "r")
  (let ((text (buffer-substring-no-properties start end)))
    (comint-send-string (nickle--process)
                        (if (string-suffix-p "\n" text) text (concat text "\n")))))

(defun nickle-send-buffer ()
  "Send the whole buffer to Nickle, as if typed."
  (interactive)
  (nickle-send-region (point-min) (point-max)))

(defun nickle-load-file ()
  "Save the buffer's file and have Nickle load it."
  (interactive)
  (unless buffer-file-name
    (user-error "This buffer has no file to load"))
  (save-buffer)
  (comint-send-string (nickle--process)
                      (format "load %S\n" (expand-file-name buffer-file-name))))

;;; Formatting

(defun nickle-format-buffer ()
  "Reformat the buffer with nickle --pretty, keeping it if nickle fails."
  (interactive)
  (let ((out (generate-new-buffer " *nickle-format*"))
        (err (make-temp-file "nickle-format")))
    (unwind-protect
        (if (zerop (call-process-region (point-min) (point-max) nickle-program
                                        nil (list out err) nil "--pretty" "-"))
            (replace-buffer-contents out)
          (message "nickle --pretty failed: %s"
                   (with-temp-buffer
                     (insert-file-contents err)
                     (string-trim (buffer-string)))))
      (kill-buffer out)
      (delete-file err))))

;;; The mode

(defvar nickle-mode-map
  (let ((map (make-sparse-keymap)))
    (define-key map (kbd "C-c C-f") #'nickle-format-buffer)
    (define-key map (kbd "C-c C-z") #'run-nickle)
    (define-key map (kbd "C-c C-r") #'nickle-send-region)
    (define-key map (kbd "C-c C-b") #'nickle-send-buffer)
    (define-key map (kbd "C-c C-l") #'nickle-load-file)
    map)
  "Keymap for `nickle-mode'.")

;;;###autoload
(define-derived-mode nickle-mode prog-mode "Nickle"
  "Major mode for editing Nickle source files.

\\{nickle-mode-map}"
  :syntax-table nickle-mode-syntax-table
  (setq-local syntax-propertize-function nickle-syntax-propertize)
  (setq-local font-lock-defaults '(nickle-font-lock-keywords))
  (setq-local comment-start "/* ")
  (setq-local comment-end " */")
  (setq-local comment-start-skip "\\(?:/\\*+\\|^[ \t]*#+\\)[ \t]*")
  (setq-local comment-end-skip "[ \t]*\\*+/")
  (setq-local comment-use-syntax t)
  (setq-local indent-line-function #'nickle-indent-line)
  (setq-local fill-paragraph-function #'nickle-fill-paragraph)
  (setq-local indent-tabs-mode nil)
  (setq-local electric-indent-chars (append '(?} ?\) ?: ?{) electric-indent-chars)))

;;;###autoload
(add-to-list 'auto-mode-alist '("\\.5c\\'" . nickle-mode))
;;;###autoload
(add-to-list 'interpreter-mode-alist '("nickle" . nickle-mode))

(provide 'nickle-mode)

;;; nickle-mode.el ends here
