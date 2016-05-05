//==-- EpiphanyISelLowering.h - Epiphany DAG Lowering Interface ----*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the interfaces that Epiphany uses to lower LLVM code into a
// selection DAG.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TARGET_EPIPHANY_ISELLOWERING_H
#define LLVM_TARGET_EPIPHANY_ISELLOWERING_H

#include "EpiphanyConfig.h"

#include "MCTargetDesc/EpiphanyABIInfo.h"
#include "Epiphany.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/IR/Function.h"
#include "llvm/Target/TargetLowering.h"
#include <deque>

namespace llvm {
  namespace EpiphanyISD {
    enum NodeType {
      // Start the numbering from where ISD NodeType finishes.
      FIRST_NUMBER = ISD::BUILTIN_OP_END,
  
      // This is a conditional branch which also notes the flag needed
      // (eq/sgt/...). Epiphany puts this information on the branches rather than
      // compares as LLVM does.
      BR_CC,
  
      // A node to be selected to an actual call operation: either BL or BLR in
      // the absence of tail calls.
      Call,
  
      // Simply a convenient node inserted during ISelLowering to represent
      // procedure return. Will almost certainly be selected to "RET".
      Ret,
  
      /// This is an A64-ification of the standard LLVM SELECT_CC operation. The
      /// main difference is that it only has the values and an A64 condition,
      /// which will be produced by a setcc instruction.
      SELECT_CC,
  
      /// This serves most of the functions of the LLVM SETCC instruction, for two
      /// purposes. First, it prevents optimisations from fiddling with the
      /// compare after we've moved the CondCode information onto the SELECT_CC or
      /// BR_CC instructions. Second, it gives a legal instruction for the actual
      /// comparison.
      ///
      /// It keeps a record of the condition flags asked for because certain
      /// instructions are only valid for a subset of condition codes.
      SETCC,
  
      // Wraps an address which the ISelLowering phase has decided should be
      // created using the small absolute memory model: i.e. adrp/add or
      // adrp/mem-op. This exists to prevent bare TargetAddresses which may never
      // get selected.
      WrapperSmall,
  
      // Node for FMA and FMS
      FM_A_S
    };
  }

  //===--------------------------------------------------------------------===//
  // TargetLowering Implementation
  //===--------------------------------------------------------------------===//
  class EpiphanyMachineFunctionInfo;
  class EpiphanySubtarget;
  
  class EpiphanyTargetLowering : public TargetLowering {
  public:
    explicit EpiphanyTargetLowering(const EpiphanyTargetMachine &TM, 
                                    const EpiphanySubtarget &STI);

    /// getTargetNodeName - This method returns the name of a target specific
    //  DAG node.
    const char *getTargetNodeName(unsigned Opcode) const override;
    
    SDValue LowerOperation(SDValue Op, SelectionDAG &DAG) const override;

  protected:
    /// ByValArgInfo - Byval argument information.
    struct ByValArgInfo {
      unsigned FirstIdx; // Index of the first register used.
      unsigned NumRegs;  // Number of registers used for this argument.
      unsigned Address;  // Offset of the stack area used to pass this argument.

      ByValArgInfo() : FirstIdx(0), NumRegs(0), Address(0) {}
    };

    // Subtarget Info
    const EpiphanySubtarget &Subtarget;
    // Cache the ABI from the TargetMachine, we use it everywhere.
    const EpiphanyABIInfo &ABI;
    
  private:
  
    // Lower Operand specifics
    SDValue lowerGlobalAddress(SDValue Op, SelectionDAG &DAG) const;
    
    //- must be exist even without function all
    SDValue LowerFormalArguments(SDValue Chain,
                                 CallingConv::ID CallConv, bool isVarArg,
                                 const SmallVectorImpl<ISD::InputArg> &Ins,
                                 SDLoc dl, SelectionDAG &DAG,
                                 SmallVectorImpl<SDValue> &InVals) const override;

    SDValue LowerReturn(SDValue Chain,
                        CallingConv::ID CallConv, bool isVarArg,
                        const SmallVectorImpl<ISD::OutputArg> &Outs,
                        const SmallVectorImpl<SDValue> &OutVals,
                        SDLoc dl, SelectionDAG &DAG) const override;

  };
} // namespace llvm

#endif // LLVM_TARGET_EPIPHANY_ISELLOWERING_H