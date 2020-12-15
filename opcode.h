/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
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
    OpTagGlobal,
    OpTagLocal,
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
    OpBuildArrayInd,
    OpInitArray,
    OpBuildHash,
    OpInitHash,
    OpInitHashDef,
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
    OpIsType,
    OpHasMember,
    OpEnd,
    OpDrop
} __attribute__ ((packed)) OpCode;

#endif /* _CODE_H_ */
