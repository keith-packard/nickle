/*
 * $Id$
 *
 * Copyright 1996 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _CODE_H_
#define _CODE_H_

typedef enum _OpCode {
    OpNoop,
    /*
     * Statement op codes
     */
    OpIf, 
    OpElse,
    OpWhile, 
    OpEndWhile,
    OpDo,
    OpFor, 
    OpEndFor,
    OpBreak, 
    OpContinue,
    OpReturn,
    OpFunction,
    OpFork,
    /*
     * Expr op codes
     */
    OpName,
    OpNameRef,
    OpConst,
    OpBuildArray,
    OpInitArray,
    OpInitStruct,
    OpArray,
    OpArrayRef,
    OpCall,
    OpDot,
    OpDotRef,
    OpArrow,
    OpArrowRef,
    OpObj,
    OpStaticInit,
    OpStaticDone,
    OpStar,
    OpUminus,
    OpLnot,
    OpBang,
    OpFact,
    OpPreInc, 
    OpPostInc,
    OpPreDec, 
    OpPostDec,
    OpPlus,
    OpMinus,
    OpTimes,
    OpDivide,
    OpDiv,
    OpMod,
    OpPow,
    OpQuest, 
    OpColon,
    OpLxor,
    OpLand, 
    OpLor,
    OpAnd, 
    OpOr,
    OpAssign,
    OpAssignPlus, 
    OpAssignMinus,
    OpAssignTimes,
    OpAssignDivide,
    OpAssignDiv,
    OpAssignMod,
    OpAssignPow,
    OpAssignLxor,
    OpAssignLand,
    OpAssignLor,
    OpEq,
    OpNe,
    OpLt,
    OpGt,
    OpLe,
    OpGe,
    OpEnd,
} OpCode;

#endif /* _CODE_H_ */
