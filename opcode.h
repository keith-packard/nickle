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
    OpBranch,
    OpBranchFalse,
    OpBranchTrue,
    OpCase,
    OpTagCase,
    OpTagStore,
    OpDefault,
    OpReturn,
    OpReturnVoid,
    OpFork,
    OpCatch,
    OpEndCatch,
    OpRaise,
    OpTwixt,
    OpTwixtDone,
    OpEnterDone,
    OpLeaveDone,
    OpFarJump,
    OpUnwind,
    /*
     * Expr op codes
     */
    OpGlobal,
    OpGlobalRef,
    OpGlobalRefStore,
    OpStatic,
    OpStaticRef,
    OpStaticRefStore,
    OpLocal,
    OpLocalRef,
    OpLocalRefStore,
    OpFetch,
    OpConst,
    OpBuildArray,
    OpInitArray,
    OpBuildHash,
    OpInitHash,
    OpBuildStruct,
    OpInitStruct,
    OpBuildUnion,
    OpInitUnion,
    OpArray,
    OpArrayRef,
    OpArrayRefStore,
    OpVarActual,
    OpCall,
    OpTailCall,
    OpExceptionCall,
    OpDot,
    OpDotRef,
    OpDotRefStore,
    OpArrow,
    OpArrowRef,
    OpArrowRefStore,
    OpObj,
    OpStaticInit,
    OpStaticDone,
    OpBinOp,
    OpBinFunc,
    OpUnOp,
    OpUnFunc,
    OpPreOp,
    OpPostOp,
    OpAssign,
    OpAssignOp,
    OpAssignFunc,
    OpEnd,
} __attribute__ ((packed)) OpCode;

#endif /* _CODE_H_ */
