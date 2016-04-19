
//===-- EpiphanySubtarget.cpp - Epiphany Subtarget Information --------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the Epiphany specific subclass of TargetSubtargetInfo.
//
//===----------------------------------------------------------------------===//

#include "EpiphanySubtarget.h"

#include "EpiphanyMachineFunctionInfo.h"
#include "Epiphany.h"
#include "EpiphanyRegisterInfo.h"

#include "EpiphanyTargetMachine.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/TargetRegistry.h"

using namespace llvm;

#define DEBUG_TYPE "epiphany-subtarget"

#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR
#include "EpiphanyGenSubtargetInfo.inc"

/// Select the Epiphany CPU for the given triple and cpu name.
static StringRef selectEpiphanyCPU(Triple TT, StringRef CPU) {
  if (CPU.empty() || CPU == "generic") {
    if (TT.getArch() == Triple::epiphany)
      CPU = "epiphany";
  }
  return CPU;
}

void EpiphanySubtarget::anchor() {}

EpiphanySubtarget::EpiphanySubtarget(const Triple &TT, const std::string &CPU, 
                                     const std::string &FS, const TargetMachine &_TM)
  :   EpiphanyGenSubtargetInfo(TT, CPU, FS), 
      TM(_TM), TargetTriple(TT), TSInfo(), FrameLowering(),
      InstrInfo(initializeSubtargetDependencies(CPU, FS, TM)), 
      TLInfo(TM, *this) {}

EpiphanySubtarget &
EpiphanySubtarget::initializeSubtargetDependencies(StringRef CPU, StringRef FS,
                                                   const TargetMachine &TM) {
  // Get CPU name string
  std::string CPUName = selectEpiphanyCPI(TargetTriple, CPU);
  // E16 doesn't have built-in CMP function
  if (isE16()) {
    HasCmp = false;
  }
  
  // Parse features string
  ParseSubtargetFeatures(CPUName, FS);
  // Initialize scheduling itinerary for the specified CPU.
  InstrItins = getInstrItineraryForCPU(CPUName);
  
  return *this;
}

const EpiphanyABIInfo &EpiphanySubtarget::getABI() const { return TM.getABI(); }