//===-- EpiphanyAsmParser.cpp - Parse Epiphany assembly to MCInst instructions ----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "EpiphanyAsmParser.h"

namespace {

  /// EpiphanyOperand - Instances of this class represent a parsed Epiphany machine
  /// instruction.
  class EpiphanyOperand : public MCParsedAsmOperand {

    enum KindTy {
      k_CondCode,
      k_CoprocNum,
      k_Immediate,
      k_Memory,
      k_IdxMemory,
      k_Register,
      k_Token
    } Kind;

  public:
    EpiphanyOperand(KindTy K) : MCParsedAsmOperand(), Kind(K) {}

    struct Token {
      const char *Data;
      unsigned Length;
    };
    struct PhysRegOp {
      unsigned RegNum; /// Register Number
    };
    struct ImmOp {
      const MCExpr *Val;
    };
    struct MemOp {
      unsigned Base;
      const MCExpr *Off;
    };
    struct IdxMemOp {
      unsigned Base;
      unsigned Offset;
    };

    union {
      struct Token Tok;
      struct PhysRegOp Reg;
      struct ImmOp Imm;
      struct MemOp Mem;
      struct IdxMemOp IdxMem;
    };

    SMLoc StartLoc, EndLoc;

  public:
    void addRegOperands(MCInst &Inst, unsigned N) const {
      assert(N == 1 && "Invalid number of operands!");
      Inst.addOperand(MCOperand::createReg(getReg()));
    }

    void addExpr(MCInst &Inst, const MCExpr *Expr) const {
      // Add as immediate when possible.  Null MCExpr = 0.
      if (Expr == 0)
        Inst.addOperand(MCOperand::createImm(0));
      else if (const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(Expr))
        Inst.addOperand(MCOperand::createImm(CE->getValue()));
      else
        Inst.addOperand(MCOperand::createExpr(Expr));
    }

    void addImmOperands(MCInst &Inst, unsigned N) const {
      assert(N == 1 && "Invalid number of operands!");
      const MCExpr *Expr = getImm();
      addExpr(Inst, Expr);
    }

    void addMemOperands(MCInst &Inst, unsigned N) const {
      assert(N == 2 && "Invalid number of operands!");

      Inst.addOperand(MCOperand::createReg(getMemBase()));

      const MCExpr *Expr = getMemOff();
      addExpr(Inst, Expr);
    }

    void addIdxMemOperands(MCInst &Inst, unsigned N) const {
      assert(N == 2 && "Invalid number of operands!");

      Inst.addOperand(MCOperand::createReg(getIdxMemBase()));
      Inst.addOperand(MCOperand::createReg(getIdxMemOffset()));
    }

    bool isReg() const { return Kind == k_Register; }

    bool isImm() const { return Kind == k_Immediate; }

    bool isToken() const { return Kind == k_Token; }

    bool isMem() const { return Kind == k_Memory; }

    bool isIdxMem() const { return Kind == k_IdxMemory; }

    bool isConstantImm() const {
      return isImm() && isa<MCConstantExpr>(getImm());
    }

    // Additional templates for different int sizes
    template<unsigned Bits, int Offset = 0>
    bool isConstantUImm() const {
      return isConstantImm() && isUInt<Bits>(getConstantImm() - Offset);
    }

    template<unsigned Bits>
    bool isSImm() const {
      return isConstantImm() ? isInt<Bits>(getConstantImm()) : isImm();
    }

    template<unsigned Bits>
    bool isUImm() const {
      return isConstantImm() ? isUInt<Bits>(getConstantImm()) : isImm();
    }

    template<unsigned Bits>
    bool isAnyImm() const {
      return isConstantImm() ? (isInt<Bits>(getConstantImm()) ||
                                isUInt<Bits>(getConstantImm()))
                             : isImm();
    }

    template<unsigned Bits, int Offset = 0>
    bool isConstantSImm() const {
      return isConstantImm() && isInt<Bits>(getConstantImm() - Offset);
    }

    template<unsigned Bottom, unsigned Top>
    bool isConstantUImmRange() const {
      return isConstantImm() && getConstantImm() >= Bottom &&
             getConstantImm() <= Top;
    }

    StringRef getToken() const {
      assert(Kind == k_Token && "Invalid access!");
      return StringRef(Tok.Data, Tok.Length);
    }

    unsigned getReg() const {
      assert((Kind == k_Register) && "Invalid access!");
      return Reg.RegNum;
    }

    const MCExpr *getImm() const {
      assert((Kind == k_Immediate) && "Invalid access!");
      return Imm.Val;
    }

    int64_t getConstantImm() const {
      const MCExpr *Val = getImm();
      return static_cast<const MCConstantExpr *>(Val)->getValue();
    }

    unsigned getMemBase() const {
      assert((Kind == k_Memory) && "Invalid access!");
      return Mem.Base;
    }

    const MCExpr *getMemOff() const {
      assert((Kind == k_Memory) && "Invalid access!");
      return Mem.Off;
    }

    unsigned getIdxMemBase() const {
      assert((Kind == k_IdxMemory) && "Invalid access!");
      return IdxMem.Base;
    }

    unsigned getIdxMemOffset() const {
      assert((Kind == k_IdxMemory) && "Invalid access!");
      return IdxMem.Offset;
    }

    static std::unique_ptr<EpiphanyOperand> CreateToken(StringRef Str, SMLoc S) {
      auto Op = make_unique<EpiphanyOperand>(k_Token);
      Op->Tok.Data = Str.data();
      Op->Tok.Length = Str.size();
      Op->StartLoc = S;
      Op->EndLoc = S;
      return Op;
    }

    /// Internal constructor for register kinds
    static std::unique_ptr<EpiphanyOperand> CreateReg(unsigned RegNum, SMLoc S,
                                                      SMLoc E) {
      auto Op = make_unique<EpiphanyOperand>(k_Register);
      Op->Reg.RegNum = RegNum;
      Op->StartLoc = S;
      Op->EndLoc = E;
      return Op;
    }

    static std::unique_ptr<EpiphanyOperand> CreateImm(const MCExpr *Val, SMLoc S, SMLoc E) {
      auto Op = make_unique<EpiphanyOperand>(k_Immediate);
      Op->Imm.Val = Val;
      Op->StartLoc = S;
      Op->EndLoc = E;
      return Op;
    }

    static std::unique_ptr<EpiphanyOperand> CreateMem(unsigned Base, const MCExpr *Off,
                                                      SMLoc S, SMLoc E) {
      auto Op = make_unique<EpiphanyOperand>(k_Memory);
      Op->Mem.Base = Base;
      Op->Mem.Off = Off;
      Op->StartLoc = S;
      Op->EndLoc = E;
      return Op;
    }

    static std::unique_ptr<EpiphanyOperand> CreateIdxMem(unsigned Base, unsigned Offset,
                                                         SMLoc S, SMLoc E) {
      auto Op = make_unique<EpiphanyOperand>(k_IdxMemory);
      Op->IdxMem.Base = Base;
      Op->IdxMem.Offset = Offset;
      Op->StartLoc = S;
      Op->EndLoc = E;
      return Op;
    }

    /// getStartLoc - Get the location of the first token of this operand.
    SMLoc getStartLoc() const override { return StartLoc; }

    /// getEndLoc - Get the location of the last token of this operand.
    SMLoc getEndLoc() const override { return EndLoc; }

    void printReg(raw_ostream &OS) const {
      OS << "Register: " << Reg.RegNum << "\n";
    }

    void printImm(raw_ostream &OS) const {
      OS << "Immediate: " << Imm.Val << "\n";
    }

    void printToken(raw_ostream &OS) const {
      OS << "Token: " << Tok.Data << "\n";
    }

    void printMem(raw_ostream &OS) const {
      OS << "Memory addr: base " << Mem.Base << ", offset ";
      Mem.Off->dump();
    }

    void printIdxMem(raw_ostream &OS) const {
      OS << "Indexed memory addr: base " << IdxMem.Base << ", offset " << IdxMem.Offset << "\n";
    }

    void print(raw_ostream &OS) const override {
      if (isReg())
        printReg(OS);
      if (isImm())
        printImm(OS);
      if (isToken())
        printToken(OS);
      if (isMem())
        printMem(OS);
      if (isIdxMem())
        printIdxMem(OS);
    }
  };
}

//@1 {
// Some ops may need expansion
bool EpiphanyAsmParser::needsExpansion(MCInst &Inst) {

  switch (Inst.getOpcode()) {
    default:
      return false;
  }
}

void EpiphanyAsmParser::expandInstruction(MCInst &Inst, SMLoc IDLoc,
                                          SmallVectorImpl<MCInst> &Instructions) {
  switch (Inst.getOpcode()) {
    default:
      break;
  }
}
//@1 }

//@2 {
bool EpiphanyAsmParser::MatchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
                                                OperandVector &Operands,
                                                MCStreamer &Out,
                                                uint64_t &ErrorInfo,
                                                bool MatchingInlineAsm) {
  MCInst Inst;
  unsigned MatchResult = MatchInstructionImpl(Operands, Inst, ErrorInfo,
                                              MatchingInlineAsm);
  switch (MatchResult) {
    default:
      break;
    case Match_Success: {
      if (needsExpansion(Inst)) {
        SmallVector<MCInst, 4> Instructions;
        expandInstruction(Inst, IDLoc, Instructions);
        for (unsigned i = 0; i < Instructions.size(); i++) {
          Out.EmitInstruction(Instructions[i], getSTI());
        }
      } else {
        Inst.setLoc(IDLoc);
        Out.EmitInstruction(Inst, getSTI());
      }
      return false;
    }
      //@2 }
    case Match_MissingFeature:
      Error(IDLoc, "instruction requires a CPU feature not currently enabled");
      return true;
    case Match_InvalidOperand: {
      SMLoc ErrorLoc = IDLoc;
      if (ErrorInfo != ~0U) {
        if (ErrorInfo >= Operands.size())
          return Error(IDLoc, "too few operands for instruction");

        ErrorLoc = ((EpiphanyOperand &) *Operands[ErrorInfo]).getStartLoc();
        if (ErrorLoc == SMLoc()) ErrorLoc = IDLoc;
      }

      return Error(ErrorLoc, "invalid operand for instruction");
    }
    case Match_MnemonicFail:
      return Error(IDLoc, "invalid instruction");
  }
  return true;
}

// Get register by number
unsigned EpiphanyAsmParser::getReg(int RC, int RegNo) {
  return *(getContext().getRegisterInfo()->getRegClass(RC).begin() + RegNo);
}

// Match register by number
int EpiphanyAsmParser::matchRegisterByNumber(unsigned RegNum, StringRef Mnemonic) {
  if (RegNum > 63)
    return -1;

  return getReg(Epiphany::GPR32RegClassID, RegNum);
}

int EpiphanyAsmParser::tryParseRegister(StringRef Mnemonic) {
  const AsmToken &Tok = Parser.getTok();
  int RegNum = -1;

  if (Tok.is(AsmToken::Identifier)) {
    // Matching with upper-case, as all regs are defined this way
    RegNum = MatchRegisterName(Tok.getString().upper());
  } else if (Tok.is(AsmToken::Integer))
    // In some cases we might even get pure integer
    RegNum = matchRegisterByNumber(static_cast<unsigned>(Tok.getIntVal()),
                                   Mnemonic.lower());
  else
    llvm_unreachable(strcat("Can't parse register: ", Mnemonic.data()));
  return RegNum;
}

bool EpiphanyAsmParser::tryParseRegisterOperand(OperandVector &Operands,
                                                StringRef Mnemonic) {

  SMLoc S = Parser.getTok().getLoc();
  int RegNo = -1;

  RegNo = tryParseRegister(Mnemonic);
  if (RegNo == -1)
    return true;

  Operands.push_back(EpiphanyOperand::CreateReg(RegNo, S,
                                                Parser.getTok().getLoc()));
  Parser.Lex(); // Eat register token.
  return false;
}

bool EpiphanyAsmParser::ParseOperand(OperandVector &Operands,
                                     StringRef Mnemonic) {
  DEBUG(dbgs() << "ParseOperand\n");
  // Check if the current operand has a custom associated parser, if so, try to
  // custom parse the operand, or fallback to the general approach.
  OperandMatchResultTy ResTy = MatchOperandParserImpl(Operands, Mnemonic);
  if (ResTy == MatchOperand_Success)
    return false;
  // If there wasn't a custom match, try the generic matcher below. Otherwise,
  // there was a match, but an error occurred, in which case, just return that
  // the operand parsing failed.
  if (ResTy == MatchOperand_ParseFail)
    return true;

  DEBUG(dbgs() << ".. Generic Parser\n");

  switch (getLexer().getKind()) {
    default:
      Error(Parser.getTok().getLoc(), "unexpected token in operand");
      return true;
    case AsmToken::RBrac:
    case AsmToken::LBrac: {
      // Just add brackets into op list and continue processing the register
      SMLoc S = Parser.getTok().getLoc();
      StringRef string = Parser.getTok().getString();
      Operands.push_back(EpiphanyOperand::CreateToken(string, S));
      Parser.Lex(); // Eat the bracket
      return false;
    }
    case AsmToken::Identifier: {
      // try if it is register
      SMLoc S = Parser.getTok().getLoc();
      // parse register operand
      if (!tryParseRegisterOperand(Operands, Mnemonic)) {
        return false;
      }
      // maybe it is a symbol reference
      StringRef Identifier;
      if (Parser.parseIdentifier(Identifier))
        return true;

      SMLoc E = SMLoc::getFromPointer(Parser.getTok().getLoc().getPointer() - 1);

      MCSymbol *Sym = getContext().getOrCreateSymbol(Identifier);

      // Otherwise create a symbol ref.
      const MCExpr *Res = MCSymbolRefExpr::create(Sym, MCSymbolRefExpr::VK_None,
                                                  getContext());

      Operands.push_back(EpiphanyOperand::CreateImm(Res, S, E));
      return false;
    }
    case AsmToken::Hash:
      // Integers start with hash, strip it
      Parser.Lex();
    case AsmToken::LParen:
    case AsmToken::Minus:
    case AsmToken::Plus:
    case AsmToken::Integer:
    case AsmToken::String: {
      // quoted label names
      const MCExpr *IdVal;
      SMLoc S = Parser.getTok().getLoc();
      if (getParser().parseExpression(IdVal))
        return true;
      SMLoc E = SMLoc::getFromPointer(Parser.getTok().getLoc().getPointer() - 1);
      Operands.push_back(EpiphanyOperand::CreateImm(IdVal, S, E));
      return false;
    }
    case AsmToken::Percent: {
      // it is a symbol reference or constant expression
      const MCExpr *IdVal;
      SMLoc S = Parser.getTok().getLoc(); // start location of the operand
      if (parseRelocOperand(IdVal))
        return true;

      SMLoc E = SMLoc::getFromPointer(Parser.getTok().getLoc().getPointer() - 1);

      Operands.push_back(EpiphanyOperand::CreateImm(IdVal, S, E));
      return false;
    } // case AsmToken::Percent
  } // switch(getLexer().getKind())
  return true;
}

///@evaluateRelocExpr
// This function parses expressions like %high(imm) and transforms them to reloc
const MCExpr *EpiphanyAsmParser::evaluateRelocExpr(const MCExpr *Expr,
                                                   StringRef RelocStr) {
  EpiphanyMCExpr::EpiphanyExprKind Kind =
      StringSwitch<EpiphanyMCExpr::EpiphanyExprKind>(RelocStr)
          .Case("high", EpiphanyMCExpr::CEK_HIGH)
          .Case("low", EpiphanyMCExpr::CEK_LOW)
          .Default(EpiphanyMCExpr::CEK_None);

  assert(Kind != EpiphanyMCExpr::CEK_None);
  return EpiphanyMCExpr::create(Kind, Expr, getContext());
}

// Parse relocation expression
// For example, %high(smth)
bool EpiphanyAsmParser::parseRelocOperand(const MCExpr *&Res) {

  Parser.Lex(); // eat % token
  const AsmToken &Tok = Parser.getTok(); // get next token, operation
  if (Tok.isNot(AsmToken::Identifier))
    return true;

  std::string Str = Tok.getIdentifier().str();

  Parser.Lex(); // eat identifier
  // now make expression from the rest of the operand
  const MCExpr *IdVal;
  SMLoc EndLoc;

  if (getLexer().getKind() == AsmToken::LParen) {
    while (1) {
      Parser.Lex(); // eat '(' token
      if (getLexer().getKind() == AsmToken::Percent) {
        Parser.Lex(); // eat % token
        const AsmToken &nextTok = Parser.getTok();
        if (nextTok.isNot(AsmToken::Identifier))
          return true;
        Str += "(%";
        Str += nextTok.getIdentifier();
        Parser.Lex(); // eat identifier
        if (getLexer().getKind() != AsmToken::LParen)
          return true;
      } else
        break;
    }
    if (getParser().parseParenExpression(IdVal, EndLoc))
      return true;

    while (getLexer().getKind() == AsmToken::RParen)
      Parser.Lex(); // eat ')' token

  } else
    return true; // parenthesis must follow reloc operand

  Res = evaluateRelocExpr(IdVal, Str);
  return false;
}

bool EpiphanyAsmParser::ParseRegister(unsigned &RegNo, SMLoc &StartLoc,
                                      SMLoc &EndLoc) {

  StartLoc = Parser.getTok().getLoc();
  RegNo = tryParseRegister("");
  EndLoc = Parser.getTok().getLoc();
  return (RegNo == (unsigned) -1);
}

bool EpiphanyAsmParser::parseMemOffset(const MCExpr *&Res) {

  SMLoc S;

  switch (getLexer().getKind()) {
    default:
      return true;
    case AsmToken::Integer:
    case AsmToken::Minus:
    case AsmToken::Plus:
      return (getParser().parseExpression(Res));
    case AsmToken::Percent:
      return parseRelocOperand(Res);
    case AsmToken::LParen:
      return false;  // it's probably assuming 0
  }
  return true;
}

/// parseMemOperand
// eg, [r0, #1] or [r0, r1]
OperandMatchResultTy EpiphanyAsmParser::parseMemOperand(
    OperandVector &Operands) {

  bool isIndex = true;
  const MCExpr *IdVal = 0;
  SMLoc S;

  if (Parser.getTok().isNot(AsmToken::LBrac)) {
    // If there's no LBrac - that's not a mem operand. It might happen with addition
    return MatchOperand_NoMatch;
  }
  Parser.Lex(); // Eat '[' token

  // first operand is the register
  S = Parser.getTok().getLoc();
  if (tryParseRegisterOperand(Operands, "")) {
    Error(Parser.getTok().getLoc(), "unexpected token in mem operand, expected register");
    return MatchOperand_ParseFail;
  }

  // If offset is not null, the register is followed by comma
  SMLoc E;
  if (Parser.getTok().is(AsmToken::Comma)) {

    Parser.Lex(); // Eat ',' token
    // second operand is the offset
    // if it starts with Hash - it's an immediate
    if (Parser.getTok().is(AsmToken::Hash)) {
      isIndex = false;
      Parser.Lex(); // Eat '#' token
      if (parseMemOffset(IdVal)) {
        Error(Parser.getTok().getLoc(), "unexpected token in mem operand, expected immediate");
        return MatchOperand_ParseFail;
      }
    } else if (tryParseRegisterOperand(Operands, "")) {
      // If not - it should be register
      Error(Parser.getTok().getLoc(), "unexpected token in mem operand, expected register or immediate");
      return MatchOperand_ParseFail;
    }
    if (Parser.getTok().isNot(AsmToken::RBrac)) {
      Error(Parser.getTok().getLoc(), "unexpected token in mem operand, expected RBrac");
      return MatchOperand_ParseFail;
    }
    E = SMLoc::getFromPointer(Parser.getTok().getLoc().getPointer() - 1);
    Parser.Lex(); // Eat ']' token.

  } else if (Parser.getTok().is(AsmToken::RBrac)) {

    // If postmod load/store
    Parser.Lex(); // Eat ']' token
    if (Parser.getTok().isNot(AsmToken::Comma)) {
      Error(Parser.getTok().getLoc(), "unexpected token in mem operand, expected comma");
      return MatchOperand_ParseFail;
    }
    Parser.Lex(); // Eat ',' token
    E = SMLoc::getFromPointer(Parser.getTok().getLoc().getPointer() - 1);
    if (Parser.getTok().is(AsmToken::Hash)) {
      isIndex = false;
      Parser.Lex(); // Eat '#' token
      if (parseMemOffset(IdVal)) {
        Error(Parser.getTok().getLoc(), "unexpected token in mem operand, expected immediate");
        return MatchOperand_ParseFail;
      }
    } else if (tryParseRegisterOperand(Operands, "")) {
      // If not - it should be register
      Error(Parser.getTok().getLoc(), "unexpected token in mem operand, expected register or immediate");
      return MatchOperand_ParseFail;
    }

  } else {

    Error(Parser.getTok().getLoc(), "unexpected token in mem operand, expected RBrac");
    return MatchOperand_ParseFail;

  }

  // If offset is not a register - create Memory operand
  if (!isIndex) {
    if (!IdVal) {
      IdVal = MCConstantExpr::create(0, getContext());
    }

    // Replace the register operand with the memory operand.
    DEBUG(dbgs() << "Replacing operand with mem, old operands vector size: " << Operands.size(););
    std::unique_ptr<EpiphanyOperand> op(
        static_cast<EpiphanyOperand *>(Operands.back().release()));
    int RegNo = op->getReg();
    // remove both register and offset from operands
    Operands.pop_back();

    // Create memory operand out of base and offset
    Operands.push_back(EpiphanyOperand::CreateMem(RegNo, IdVal, S, E));
    DEBUG(dbgs() << ", new operands vector size: " << Operands.size() << "\n";);
  }

  // If offset is a reg - do nothing, the reg is already pushed into the ops list

  // Debug info
  for (unsigned idx = 0; idx < Operands.size(); idx++) {
    Operands[idx]->dump();
  }
  return MatchOperand_Success;
}

bool EpiphanyAsmParser::parseMathOperation(StringRef Name, SMLoc NameLoc,
                                           OperandVector &Operands) {
  // split the format
  size_t Start = Name.find('.'), Next = Name.rfind('.');
  StringRef Format1 = Name.slice(Start, Next);
  // and add the first format to the operands
  Operands.push_back(EpiphanyOperand::CreateToken(Format1, NameLoc));
  // now for the second format
  StringRef Format2 = Name.slice(Next, StringRef::npos);
  Operands.push_back(EpiphanyOperand::CreateToken(Format2, NameLoc));

  // set the format for the first register
  //  setFpFormat(Format1);

  // Read the remaining operands.
  if (getLexer().isNot(AsmToken::EndOfStatement)) {
    // Read the first operand.
    if (ParseOperand(Operands, Name)) {
      SMLoc Loc = getLexer().getLoc();
      Parser.eatToEndOfStatement();
      return Error(Loc, "unexpected token in argument list");
    }

    if (getLexer().isNot(AsmToken::Comma)) {
      SMLoc Loc = getLexer().getLoc();
      Parser.eatToEndOfStatement();
      return Error(Loc, "unexpected token in argument list");

    }
    Parser.Lex();  // Eat the comma.

    // Parse and remember the operand.
    if (ParseOperand(Operands, Name)) {
      SMLoc Loc = getLexer().getLoc();
      Parser.eatToEndOfStatement();
      return Error(Loc, "unexpected token in argument list");
    }
  }

  if (getLexer().isNot(AsmToken::EndOfStatement)) {
    SMLoc Loc = getLexer().getLoc();
    Parser.eatToEndOfStatement();
    return Error(Loc, "unexpected token in argument list");
  }

  Parser.Lex(); // Consume the EndOfStatement
  return false;
}

bool EpiphanyAsmParser::
ParseInstruction(ParseInstructionInfo &Info, StringRef Name, SMLoc NameLoc,
                 OperandVector &Operands) {

  // Create the leading tokens for the mnemonic, split by '.' characters.
  size_t Start = 0, Next = Name.find('.');
  StringRef Mnemonic = Name.slice(Start, Next);
  // Refer to the explanation in source code of function DecodeJumpFR(...) in 
  // EpiphanyDisassembler.cpp
  if (Mnemonic == "ret")
    Mnemonic = "jr";

  Operands.push_back(EpiphanyOperand::CreateToken(Mnemonic, NameLoc));

  // Read the remaining operands.
  if (getLexer().isNot(AsmToken::EndOfStatement)) {
    // Read the first operand.
    if (ParseOperand(Operands, Name)) {
      SMLoc Loc = getLexer().getLoc();
      Parser.eatToEndOfStatement();
      return Error(Loc, "unexpected token in argument list");
    }

    while (getLexer().isNot(AsmToken::EndOfStatement)) {
      if (getLexer().is(AsmToken::Comma)) {
        Parser.Lex();  // Eat the comma.
      }

      // Parse and remember the operand.
      if (ParseOperand(Operands, Name)) {
        SMLoc Loc = getLexer().getLoc();
        Parser.eatToEndOfStatement();
        return Error(Loc, "unexpected token in argument list");
      }
    }
  }

  if (getLexer().isNot(AsmToken::EndOfStatement)) {
    SMLoc Loc = getLexer().getLoc();
    Parser.eatToEndOfStatement();
    return Error(Loc, "unexpected token in argument list");
  }

  Parser.Lex(); // Consume the EndOfStatement
  return false;
}

bool EpiphanyAsmParser::reportParseError(StringRef ErrorMsg) {
  SMLoc Loc = getLexer().getLoc();
  Parser.eatToEndOfStatement();
  return Error(Loc, ErrorMsg);
}

bool EpiphanyAsmParser::parseSetReorderDirective() {
  Parser.Lex();
  // if this is not the end of the statement, report error
  if (getLexer().isNot(AsmToken::EndOfStatement)) {
    reportParseError("unexpected token in statement");
    return false;
  }
  Options.setReorder();
  Parser.Lex(); // Consume the EndOfStatement
  return false;
}

bool EpiphanyAsmParser::parseSetNoReorderDirective() {
  Parser.Lex();
  // if this is not the end of the statement, report error
  if (getLexer().isNot(AsmToken::EndOfStatement)) {
    reportParseError("unexpected token in statement");
    return false;
  }
  Options.setNoreorder();
  Parser.Lex(); // Consume the EndOfStatement
  return false;
}

bool EpiphanyAsmParser::parseSetMacroDirective() {
  Parser.Lex();
  // if this is not the end of the statement, report error
  if (getLexer().isNot(AsmToken::EndOfStatement)) {
    reportParseError("unexpected token in statement");
    return false;
  }
  Options.setMacro();
  Parser.Lex(); // Consume the EndOfStatement
  return false;
}

bool EpiphanyAsmParser::parseSetNoMacroDirective() {
  Parser.Lex();
  // if this is not the end of the statement, report error
  if (getLexer().isNot(AsmToken::EndOfStatement)) {
    reportParseError("`noreorder' must be set before `nomacro'");
    return false;
  }
  if (Options.isReorder()) {
    reportParseError("`noreorder' must be set before `nomacro'");
    return false;
  }
  Options.setNomacro();
  Parser.Lex(); // Consume the EndOfStatement
  return false;
}

bool EpiphanyAsmParser::parseDirectiveSet() {

  // get next token
  const AsmToken &Tok = Parser.getTok();

  if (Tok.getString() == "reorder") {
    return parseSetReorderDirective();
  } else if (Tok.getString() == "noreorder") {
    return parseSetNoReorderDirective();
  } else if (Tok.getString() == "macro") {
    return parseSetMacroDirective();
  } else if (Tok.getString() == "nomacro") {
    return parseSetNoMacroDirective();
  }
  return true;
}

bool EpiphanyAsmParser::ParseDirective(AsmToken DirectiveID) {

  if (DirectiveID.getString() == ".ent") {
    // ignore this directive for now
    Parser.Lex();
    return false;
  }

  if (DirectiveID.getString() == ".end") {
    // ignore this directive for now
    Parser.Lex();
    return false;
  }

  if (DirectiveID.getString() == ".frame") {
    // ignore this directive for now
    Parser.eatToEndOfStatement();
    return false;
  }

  if (DirectiveID.getString() == ".set") {
    return parseDirectiveSet();
  }

  if (DirectiveID.getString() == ".fmask") {
    // ignore this directive for now
    Parser.eatToEndOfStatement();
    return false;
  }

  if (DirectiveID.getString() == ".mask") {
    // ignore this directive for now
    Parser.eatToEndOfStatement();
    return false;
  }

  if (DirectiveID.getString() == ".gpword") {
    // ignore this directive for now
    Parser.eatToEndOfStatement();
    return false;
  }

  return true;
}

extern "C" void LLVMInitializeEpiphanyAsmParser() {
  RegisterMCAsmParser<EpiphanyAsmParser> X(TheEpiphanyTarget);
}

#define GET_REGISTER_MATCHER
#define GET_MATCHER_IMPLEMENTATION

#include "EpiphanyGenAsmMatcher.inc"

