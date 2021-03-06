//===---- EpiphanyISelDAGToDAG.h - A Dag to Dag Inst Selector for E16 -----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines an instruction selector for the Epiphany target.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_EPIPHANY_EPIPHANYISELDAGTODAG_H
#define LLVM_LIB_TARGET_EPIPHANY_EPIPHANYISELDAGTODAG_H

#include "EpiphanyConfig.h"

#include "MCTargetDesc/EpiphanyAddressingModes.h"
#include "Epiphany.h"
#include "EpiphanySubtarget.h"
#include "EpiphanyTargetMachine.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Debug.h"

//===----------------------------------------------------------------------===//
// Instruction Selector Implementation
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// EpiphanyDAGToDAGISel - Epiphany specific code to select E16 CPU
// instructions for SelectionDAG operations.
//===----------------------------------------------------------------------===//
namespace llvm {

class EpiphanyDAGToDAGISel : public SelectionDAGISel {
public:
  explicit EpiphanyDAGToDAGISel(EpiphanyTargetMachine &TM, CodeGenOpt::Level OL)
      : SelectionDAGISel(TM, OL), Subtarget(nullptr) {}

  // Pass Name
  StringRef getPassName() const override {
    return "Epiphany DAG->DAG Pattern Instruction Selection";
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

protected:

  /// Keep a pointer to the Subtarget around so that we can make the right
  /// decision when generating code for different targets.
  const EpiphanySubtarget *Subtarget;

private:
  // Include the pieces autogenerated from the target description.
  #include "EpiphanyGenDAGISel.inc"

  /// getTargetMachine - Return a reference to the TargetMachine, casted
  /// to the target-specific type.
  const EpiphanyTargetMachine &getTargetMachine() {
    return static_cast<const EpiphanyTargetMachine &>(TM);
  }

  void Select(SDNode *N) override;

  bool trySelect(SDNode *Node);

  void processFunctionAfterISel(MachineFunction &MF);

  // Complex Pattern.
  template<bool is16bit> bool SelectAddr(SDNode *Parent, SDValue Addr, SDValue &Base, SDValue &Offset) {
    return SelectAddr(Parent, Addr, Base, Offset, is16bit);
  }
  bool SelectAddr(SDNode *Parent, SDValue N, SDValue &Base, SDValue &Offset, bool is16bit);

  // getImm - Return a target constant with the specified value.
  inline SDValue getImm(const SDNode *Node, unsigned Imm) {
    return CurDAG->getTargetConstant(Imm, SDLoc(Node), Node->getValueType(0));
  }

};

}

#endif
