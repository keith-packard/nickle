/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
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
    OpTry,
    OpEndTry,
    OpRaise,
    OpReRaise,
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
