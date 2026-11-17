;;; nickle-mode-tests.el --- Tests for nickle-mode  -*- lexical-binding: t; -*-

;; Copyright © 2026 Keith Packard and Bart Massey.
;; All Rights Reserved.  See the file COPYING in this directory
;; for licensing information.

;;; Commentary:

;; emacs --batch -Q -l nickle-mode.el -l nickle-mode-tests.el \
;;     -f ert-run-tests-batch-and-exit

;;; Code:

(require 'ert)
(require 'cl-lib)
(require 'nickle-mode)

(defconst nickle-mode-tests-directory
  (file-name-directory (or load-file-name buffer-file-name))
  "Where the tests, and the Nickle sources they indent, are.")

(defun nickle-mode-tests-comment-at (text needle)
  "Whether NEEDLE, found in TEXT, lies in a comment in `nickle-mode'."
  (with-temp-buffer
    (insert text)
    (nickle-mode)
    (goto-char (point-min))
    (search-forward needle)
    (nth 4 (syntax-ppss (match-beginning 0)))))

(ert-deftest nickle-mode-comments ()
  "Only /* */ and a '#' starting a line are comments."
  (should (nickle-mode-tests-comment-at "/* inside */\nx = 1;\n" "inside"))
  (should (nickle-mode-tests-comment-at "# inside\nx = 1;\n" "inside"))
  (should (nickle-mode-tests-comment-at "    # indented\n" "indented"))
  (should-not (nickle-mode-tests-comment-at "x = 255 # 16;\nafter;\n" "16"))
  (should-not (nickle-mode-tests-comment-at "x = 7 // 2;\nafter;\n" "2;"))
  (should-not (nickle-mode-tests-comment-at "# a comment\nafter;\n" "after")))

(defun nickle-mode-tests-reindent (text)
  "TEXT with every line indented by `nickle-mode', from no indentation."
  (with-temp-buffer
    (insert (replace-regexp-in-string "^[ \t]+" "" text))
    (nickle-mode)
    (indent-region (point-min) (point-max))
    (buffer-string)))

(defmacro nickle-mode-tests-indents (name doc text)
  "Define a test NAME, with DOC, that TEXT indents as written."
  `(ert-deftest ,name ()
     ,doc
     (should (equal (nickle-mode-tests-reindent ,text) ,text))))

(nickle-mode-tests-indents nickle-mode-indent-function
  "A definition's return type, header, doc comment, and body."
  "int
f(int a)
    /* doubles a */
{
    int b = a;

    return b * 2;
}
")

(nickle-mode-tests-indents nickle-mode-indent-namespace
  "A namespace's definitions, one level in."
  "namespace N {
    public int count = 0;

    public void
    bump()
    {
        count++;
    }
}
")

(nickle-mode-tests-indents nickle-mode-indent-switch
  "Case labels at the switch's level, their statements one in."
  "void
f(int c)
{
    switch (c) {
    case 1:
        c++;
        break;
    /* before the default */
    default:
        break;
    }
}
")

(nickle-mode-tests-indents nickle-mode-indent-control
  "Bodies without braces, else with its if, and broken headers."
  "void
f(int a, int b)
{
    if (a > b)
        a = b;
    else if (a < 0)
        a = 0;
    else
        b = a;
    while (a < 10
               && b < 10)
        a++;
    for (int i = 0; i < a; i++)
        if (i > b)
            b++;
    do
        a--;
    while (a > 0);
}
")

(nickle-mode-tests-indents nickle-mode-indent-continuation
  "Continued statements four columns in, and arguments after their parenthesis."
  "void
f(int a)
{
    string message =
        \"a long string that goes on\";
    printf(\"%d %s\\n\", a,
           message);
    int total = a * 2 + a * 3
        + a * 4;
}
")

(nickle-mode-tests-indents nickle-mode-indent-lambda
  "A lambda's block four columns past where the lambda starts."
  "poly a = func() {
             int b = 6;

             return b;
         }
")

(nickle-mode-tests-indents nickle-mode-indent-hash
  "'#' comments indent as code does."
  "# a heading
int
f()
{
    # explain
    return 1;
}
")

(ert-deftest nickle-mode-indent-sources ()
  "Nickle's own sources, laid out by nickle --format, keep their indentation."
  (dolist (name '("builtin.5c" "command.5c" "json.5c" "printf.5c" "math.5c"
                  "skiplist.5c" "parse-args.5c"))
    (let ((file (expand-file-name name nickle-mode-tests-directory)))
      (with-temp-buffer
        (insert-file-contents file)
        (let ((before (buffer-string)))
          (nickle-mode)
          (indent-region (point-min) (point-max))
          (should (equal (cons name (buffer-string)) (cons name before))))))))

(ert-deftest nickle-mode-comment-dwim ()
  "Comment commands write /* */, which can follow code."
  (with-temp-buffer
    (insert "x = 1;")
    (nickle-mode)
    (comment-dwim nil)
    (insert "note")
    (should (string-match-p "x = 1; +/\\* note \\*/" (buffer-string)))))

(defun nickle-mode-tests-fill (text needle)
  "TEXT after filling, with a fill column of 50, the paragraph at NEEDLE."
  (with-temp-buffer
    (insert text)
    (nickle-mode)
    (setq fill-column 50)
    (goto-char (point-min))
    (search-forward needle)
    (fill-paragraph)
    (buffer-string)))

(ert-deftest nickle-mode-fill-block-comment ()
  "Filling a /* */ comment keeps its ' * ' and leaves its other paragraphs."
  (should (equal (nickle-mode-tests-fill
                  "    /*
     * one two three four five six seven eight nine ten eleven twelve
     * thirteen
     *
     * second paragraph
     */
" "three")
                 "    /*
     * one two three four five six seven eight
     * nine ten eleven twelve thirteen
     *
     * second paragraph
     */
")))

(ert-deftest nickle-mode-fill-one-line-comment ()
  "A one-line comment too long to fit becomes a block; a short one stays."
  (should (equal (nickle-mode-tests-fill
                  "/* one two three four five six seven eight nine ten eleven */\n" "three")
                 "/*
 * one two three four five six seven eight nine
 * ten eleven
 */
"))
  (should (equal (nickle-mode-tests-fill "/* short */\n" "short") "/* short */\n")))

(ert-deftest nickle-mode-fill-hash-comment ()
  "Filling a '#' comment keeps its '#' marks."
  (should (equal (nickle-mode-tests-fill
                  "    # one two three four five six seven eight nine ten eleven
    # twelve
    #
    # second
" "three")
                 "    # one two three four five six seven eight nine
    # ten eleven twelve
    #
    # second
")))

(ert-deftest nickle-mode-fill-code ()
  "Filling code changes nothing."
  (should (equal (nickle-mode-tests-fill "int x = 1;\nint y = 2;\n" "x")
                 "int x = 1;\nint y = 2;\n")))

(ert-deftest nickle-mode-run-nickle ()
  "Code sent from a buffer, or a file loaded, runs in the Nickle buffer.
Runs where NICKLE names the nickle to run, as the build's tests set it."
  (skip-unless (getenv "NICKLE"))
  (let ((nickle-program (getenv "NICKLE"))
        (file (make-temp-file "nickle-mode-test" nil ".5c" "int\ntwice(int a) = a * 2;\n")))
    (unwind-protect
        (cl-flet ((output-after (thunk &optional answer)
                    ;; Run THUNK in this buffer, and wait for ANSWER, a number by default
                    (let* ((repl (get-buffer-create nickle-process-buffer))
                           (start (with-current-buffer repl (point-max)))
                           (deadline (+ (float-time) 10)))
                      (funcall thunk)
                      (with-current-buffer repl
                        (while (and (< (float-time) deadline)
                                    (not (string-match-p (or answer "[0-9]+\n> \\'")
                                                         (buffer-substring start (point-max)))))
                          (accept-process-output (get-buffer-process repl) 0.1))
                        (buffer-substring-no-properties start (point-max))))))
          (with-temp-buffer
            (insert "int y = 6;\nint\nthrice(int a)\n{\n    return a * 3;\n}\nthrice(y)\n")
            (nickle-mode)
            (should (string-match-p "18" (output-after #'nickle-send-buffer))))
          (with-current-buffer (find-file-noselect file)
            (nickle-mode)
            (output-after #'nickle-load-file "> \\'")
            (should (string-match-p
                     "42" (output-after (lambda ()
                                          (comint-send-string (nickle--process) "twice(21)\n")))))
            (kill-buffer)))
      (when (get-buffer nickle-process-buffer)
        (let ((kill-buffer-query-functions nil))
          (kill-buffer nickle-process-buffer)))
      (delete-file file))))

(provide 'nickle-mode-tests)

;;; nickle-mode-tests.el ends here
