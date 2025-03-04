//-----------------------------------------------------------------------------
// Copyright (c) 2012 GarageGames, LLC
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "console/console.h"

#include "ast.h"

#include "compiler.h"

#include "console/simBase.h"

template< typename T >
struct Token
{
   T value;
   S32 lineNumber;
};
#include "CMDgram.h"

namespace Compiler
{
   U32 compileBlock(StmtNode* block, CodeStream& codeStream, U32 ip)
   {
      for (StmtNode* walk = block; walk; walk = walk->getNext())
      {
         ip = walk->compileStmt(codeStream, ip);
      }
      return codeStream.tell();
   }
}

using namespace Compiler;

FuncVars gEvalFuncVars;
FuncVars gGlobalScopeFuncVars;
FuncVars* gFuncVars = NULL;

inline FuncVars* getFuncVars(S32 lineNumber)
{
   if (gFuncVars == &gGlobalScopeFuncVars)
   {
      const char* str = avar("Attemping to use local variable in global scope. File: %s Line: %d", CodeBlock::smCurrentParser->getCurrentFile(), lineNumber);
      scriptErrorHandler(str);
   }
   return gFuncVars;
}

//-----------------------------------------------------------------------------

void StmtNode::addBreakLine(CodeStream& code)
{
   code.addBreakLine(dbgLineNumber, code.tell());
}

//------------------------------------------------------------

StmtNode::StmtNode() : dbgLineNumber(0)
{
   next = NULL;
   dbgFileName = CodeBlock::smCurrentParser->getCurrentFile();
}

void StmtNode::setPackage(StringTableEntry)
{
}

void StmtNode::append(StmtNode* appended)
{
   StmtNode* walk = this;
   while (walk->next)
      walk = walk->next;
   walk->next = appended;
}


void FunctionDeclStmtNode::setPackage(StringTableEntry packageName)
{
   package = packageName;
}

//------------------------------------------------------------
//
// Console language compilers
//
//------------------------------------------------------------

U32 BreakStmtNode::compileStmt(CodeStream& codeStream, U32 ip)
{
   if (codeStream.inLoop())
   {
      addBreakLine(codeStream);
      codeStream.emit(OP_JMP);
      codeStream.emitFix(CodeStream::FIXTYPE_BREAK);
   }
   else
   {
      Con::warnf(ConsoleLogEntry::General, "%s (%d): break outside of loop... ignoring.", dbgFileName, dbgLineNumber);
   }
   return codeStream.tell();
}

//------------------------------------------------------------

U32 ContinueStmtNode::compileStmt(CodeStream& codeStream, U32 ip)
{
   if (codeStream.inLoop())
   {
      addBreakLine(codeStream);
      codeStream.emit(OP_JMP);
      codeStream.emitFix(CodeStream::FIXTYPE_CONTINUE);
   }
   else
   {
      Con::warnf(ConsoleLogEntry::General, "%s (%d): continue outside of loop... ignoring.", dbgFileName, dbgLineNumber);
   }
   return codeStream.tell();
}

//------------------------------------------------------------

U32 ExprNode::compileStmt(CodeStream& codeStream, U32 ip)
{
   addBreakLine(codeStream);
   return compile(codeStream, ip, TypeReqNone);
}

//------------------------------------------------------------

U32 ReturnStmtNode::compileStmt(CodeStream& codeStream, U32 ip)
{
   addBreakLine(codeStream);
   if (!expr)
      codeStream.emit(OP_RETURN_VOID);
   else
   {
      TypeReq walkType = expr->getPreferredType();
      if (walkType == TypeReqNone) walkType = TypeReqString;
      ip = expr->compile(codeStream, ip, walkType);

      // Return the correct type
      switch (walkType) {
      case TypeReqUInt:
         codeStream.emit(OP_RETURN_UINT);
         break;
      case TypeReqFloat:
         codeStream.emit(OP_RETURN_FLT);
         break;
      default:
         codeStream.emit(OP_RETURN);
         break;
      }
   }
   return codeStream.tell();
}

//------------------------------------------------------------

ExprNode* IfStmtNode::getSwitchOR(ExprNode* left, ExprNode* list, bool string)
{
   ExprNode* nextExpr = (ExprNode*)list->getNext();
   ExprNode* test;
   if (string)
      test = StreqExprNode::alloc(left->dbgLineNumber, left, list, true);
   else
      test = IntBinaryExprNode::alloc(left->dbgLineNumber, opEQ, left, list);
   if (!nextExpr)
      return test;
   return IntBinaryExprNode::alloc(test->dbgLineNumber, opOR, test, getSwitchOR(left, nextExpr, string));
}

void IfStmtNode::propagateSwitchExpr(ExprNode* left, bool string)
{
   testExpr = getSwitchOR(left, testExpr, string);
   if (propagate && elseBlock)
      ((IfStmtNode*)elseBlock)->propagateSwitchExpr(left, string);
}

U32 IfStmtNode::compileStmt(CodeStream& codeStream, U32 ip)
{
   U32 endifIp, elseIp;
   addBreakLine(codeStream);

   TypeReq testType = testExpr->getPreferredType();

   if (testType == TypeReqUInt)
   {
      integer = true;
   }
   else
   {
      integer = false;
   }

   if (testType == TypeReqString || testType == TypeReqNone)
   {
      ip = testExpr->compile(codeStream, ip, TypeReqString);
      codeStream.emit(OP_JMPNOTSTRING);
   }
   else
   {
      ip = testExpr->compile(codeStream, ip, integer ? TypeReqUInt : TypeReqFloat);
      codeStream.emit(integer ? OP_JMPIFNOT : OP_JMPIFFNOT);
   }

   if (elseBlock)
   {
      elseIp = codeStream.emit(0);
      elseOffset = compileBlock(ifBlock, codeStream, ip) + 2;
      codeStream.emit(OP_JMP);
      endifIp = codeStream.emit(0);
      endifOffset = compileBlock(elseBlock, codeStream, ip);

      codeStream.patch(endifIp, endifOffset);
      codeStream.patch(elseIp, elseOffset);
   }
   else
   {
      endifIp = codeStream.emit(0);
      endifOffset = compileBlock(ifBlock, codeStream, ip);

      codeStream.patch(endifIp, endifOffset);
   }

   // Resolve fixes
   return codeStream.tell();
}

//------------------------------------------------------------

U32 LoopStmtNode::compileStmt(CodeStream& codeStream, U32 ip)
{
   if (testExpr->getPreferredType() == TypeReqUInt)
   {
      integer = true;
   }
   else
   {
      integer = false;
   }

   // if it's a for loop or a while loop it goes:
   // initExpr
   // testExpr
   // OP_JMPIFNOT to break point
   // loopStartPoint:
   // loopBlock
   // continuePoint:
   // endLoopExpr
   // testExpr
   // OP_JMPIF loopStartPoint
   // breakPoint:

   // otherwise if it's a do ... while() it goes:
   // initExpr
   // loopStartPoint:
   // loopBlock
   // continuePoint:
   // endLoopExpr
   // testExpr
   // OP_JMPIF loopStartPoint
   // breakPoint:

   // loopBlockStart == start of loop block
   // continue == skip to end
   // break == exit loop


   addBreakLine(codeStream);
   codeStream.pushFixScope(true);

   if (initExpr)
      ip = initExpr->compile(codeStream, ip, TypeReqNone);

   if (!isDoLoop)
   {
      ip = testExpr->compile(codeStream, ip, integer ? TypeReqUInt : TypeReqFloat);
      codeStream.emit(integer ? OP_JMPIFNOT : OP_JMPIFFNOT);
      codeStream.emitFix(CodeStream::FIXTYPE_BREAK);
   }

   // Compile internals of loop.
   loopBlockStartOffset = codeStream.tell();
   continueOffset = compileBlock(loopBlock, codeStream, ip);

   if (endLoopExpr)
      ip = endLoopExpr->compile(codeStream, ip, TypeReqNone);

   ip = testExpr->compile(codeStream, ip, integer ? TypeReqUInt : TypeReqFloat);

   codeStream.emit(integer ? OP_JMPIF : OP_JMPIFF);
   codeStream.emitFix(CodeStream::FIXTYPE_LOOPBLOCKSTART);

   breakOffset = codeStream.tell(); // exit loop

   codeStream.fixLoop(loopBlockStartOffset, breakOffset, continueOffset);
   codeStream.popFixScope();

   return codeStream.tell();
}

//------------------------------------------------------------

U32 IterStmtNode::compileStmt(CodeStream& codeStream, U32 ip)
{
   // Instruction sequence:
   //
   //   containerExpr
   //   OP_ITER_BEGIN varName .fail
   // .continue:
   //   OP_ITER .break
   //   body
   //   OP_JMP .continue
   // .break:
   //   OP_ITER_END
   // .fail:

   addBreakLine(codeStream);

   codeStream.pushFixScope(true);

   bool isGlobal = varName[0] == '$';
   TypeReq varType = isStringIter ? TypeReqString : TypeReqUInt;

   const U32 startIp = ip;
   containerExpr->compile(codeStream, startIp, TypeReqString);

   codeStream.emit(isStringIter ? OP_ITER_BEGIN_STR : OP_ITER_BEGIN);
   codeStream.emit(isGlobal);
   if (isGlobal)
      codeStream.emitSTE(varName);
   else
      codeStream.emit(getFuncVars(dbgLineNumber)->assign(varName, varType, dbgLineNumber));
   const U32 finalFix = codeStream.emit(0);
   const U32 continueIp = codeStream.emit(OP_ITER);
   codeStream.emitFix(CodeStream::FIXTYPE_BREAK);
   const U32 bodyIp = codeStream.tell();

   const U32 jmpIp = compileBlock(body, codeStream, bodyIp);
   const U32 breakIp = jmpIp + 2;
   const U32 finalIp = breakIp + 1;

   codeStream.emit(OP_JMP);
   codeStream.emitFix(CodeStream::FIXTYPE_CONTINUE);
   codeStream.emit(OP_ITER_END);

   codeStream.patch(finalFix, finalIp);
   codeStream.fixLoop(bodyIp, breakIp, continueIp);
   codeStream.popFixScope();

   return codeStream.tell();
}

//------------------------------------------------------------

U32 ConditionalExprNode::compile(CodeStream& codeStream, U32 ip, TypeReq type)
{
   // code is testExpr
   // JMPIFNOT falseStart
   // trueExpr
   // JMP end
   // falseExpr
   if (testExpr->getPreferredType() == TypeReqUInt)
   {
      integer = true;
   }
   else
   {
      integer = false;
   }

   ip = testExpr->compile(codeStream, ip, integer ? TypeReqUInt : TypeReqFloat);
   codeStream.emit(integer ? OP_JMPIFNOT : OP_JMPIFFNOT);

   U32 jumpElseIp = codeStream.emit(0);
   ip = trueExpr->compile(codeStream, ip, type);
   codeStream.emit(OP_JMP);
   U32 jumpEndIp = codeStream.emit(0);
   codeStream.patch(jumpElseIp, codeStream.tell());
   ip = falseExpr->compile(codeStream, ip, type);
   codeStream.patch(jumpEndIp, codeStream.tell());

   return codeStream.tell();
}

TypeReq ConditionalExprNode::getPreferredType()
{
   // We can't make it calculate a type based on subsequent expressions as the expression
   // could be a string, or just numbers. To play it safe, stringify anything that deals with
   // a conditional, and let the interpreter cast as needed to other types safely.
   //
   // See: Regression Test 7 in ScriptTest. It has a string result in the else portion of the ?: ternary.
   return TypeReqString;
}

//------------------------------------------------------------

U32 FloatBinaryExprNode::compile(CodeStream& codeStream, U32 ip, TypeReq type)
{
   if (optimize())
   {
      ip = optimizedNode->compile(codeStream, ip, type);
      return codeStream.tell();
   }

   ip = right->compile(codeStream, ip, TypeReqFloat);
   ip = left->compile(codeStream, ip, TypeReqFloat);
   U32 operand = OP_INVALID;
   switch (op)
   {
   case '+':
      operand = OP_ADD;
      break;
   case '-':
      operand = OP_SUB;
      break;
   case '/':
      operand = OP_DIV;
      break;
   case '*':
      operand = OP_MUL;
      break;
   }
   codeStream.emit(operand);
   return codeStream.tell();
}

TypeReq FloatBinaryExprNode::getPreferredType()
{
   return TypeReqFloat;
}

//------------------------------------------------------------

void IntBinaryExprNode::getSubTypeOperand()
{
   subType = TypeReqUInt;
   switch (op)
   {
   case '^':
      operand = OP_XOR;
      break;
   case '%':
      operand = OP_MOD;
      break;
   case '&':
      operand = OP_BITAND;
      break;
   case '|':
      operand = OP_BITOR;
      break;
   case '<':
      operand = OP_CMPLT;
      subType = TypeReqFloat;
      break;
   case '>':
      operand = OP_CMPGR;
      subType = TypeReqFloat;
      break;
   case opGE:
      operand = OP_CMPGE;
      subType = TypeReqFloat;
      break;
   case opLE:
      operand = OP_CMPLE;
      subType = TypeReqFloat;
      break;
   case opEQ:
      operand = OP_CMPEQ;
      subType = TypeReqFloat;
      break;
   case opNE:
      operand = OP_CMPNE;
      subType = TypeReqFloat;
      break;
   case opOR:
      operand = OP_OR;
      break;
   case opAND:
      operand = OP_AND;
      break;
   case opSHR:
      operand = OP_SHR;
      break;
   case opSHL:
      operand = OP_SHL;
      break;
   }
}

U32 IntBinaryExprNode::compile(CodeStream& codeStream, U32 ip, TypeReq type)
{
   if (optimize())
      right = optimizedNode;

   getSubTypeOperand();

   if (operand == OP_OR || operand == OP_AND)
   {
      ip = left->compile(codeStream, ip, subType);
      codeStream.emit(operand == OP_OR ? OP_JMPIF_NP : OP_JMPIFNOT_NP);
      U32 jmpIp = codeStream.emit(0);
      ip = right->compile(codeStream, ip, subType);
      codeStream.patch(jmpIp, ip);
   }
   else
   {
      ip = right->compile(codeStream, ip, subType);
      ip = left->compile(codeStream, ip, subType);
      codeStream.emit(operand);
   }
   return codeStream.tell();
}

TypeReq IntBinaryExprNode::getPreferredType()
{
   return TypeReqUInt;
}

//------------------------------------------------------------

U32 StreqExprNode::compile(CodeStream& codeStream, U32 ip, TypeReq type)
{
   // eval str left
   // OP_ADVANCE_STR_NUL
   // eval str right
   // OP_COMPARE_STR
   // optional conversion

   ip = left->compile(codeStream, ip, TypeReqString);
   ip = right->compile(codeStream, ip, TypeReqString);
   codeStream.emit(OP_COMPARE_STR);
   if (!eq)
      codeStream.emit(OP_NOT);
   return codeStream.tell();
}

TypeReq StreqExprNode::getPreferredType()
{
   return TypeReqUInt;
}

//------------------------------------------------------------

U32 StrcatExprNode::compile(CodeStream& codeStream, U32 ip, TypeReq type)
{
   ip = left->compile(codeStream, ip, TypeReqString);
   if (appendChar)
   {
      codeStream.emit(OP_ADVANCE_STR_APPENDCHAR);
      codeStream.emit(appendChar);
   }
   ip = right->compile(codeStream, ip, TypeReqString);
   codeStream.emit(OP_REWIND_STR);
   return codeStream.tell();
}

TypeReq StrcatExprNode::getPreferredType()
{
   return TypeReqString;
}

//------------------------------------------------------------

U32 CommaCatExprNode::compile(CodeStream& codeStream, U32 ip, TypeReq type)
{
   ip = left->compile(codeStream, ip, TypeReqString);
   codeStream.emit(OP_ADVANCE_STR_APPENDCHAR);
   codeStream.emit('_');
   ip = right->compile(codeStream, ip, TypeReqString);
   codeStream.emit(OP_REWIND_STR);

   // At this point the stack has the concatenated string.

   // But we're paranoid, so accept (but whine) if we get an oddity...
   if (type == TypeReqUInt || type == TypeReqFloat)
      Con::warnf(ConsoleLogEntry::General, "%s (%d): converting comma string to a number... probably wrong.", dbgFileName, dbgLineNumber);

   return codeStream.tell();
}

TypeReq CommaCatExprNode::getPreferredType()
{
   return TypeReqString;
}

//------------------------------------------------------------

U32 IntUnaryExprNode::compile(CodeStream& codeStream, U32 ip, TypeReq type)
{
   integer = true;
   TypeReq prefType = expr->getPreferredType();
   if (op == '!' && (prefType == TypeReqFloat || prefType == TypeReqString))
      integer = false;

   ip = expr->compile(codeStream, ip, integer ? TypeReqUInt : TypeReqFloat);
   if (op == '!')
      codeStream.emit(integer ? OP_NOT : OP_NOTF);
   else if (op == '~')
      codeStream.emit(OP_ONESCOMPLEMENT);

   return codeStream.tell();
}

TypeReq IntUnaryExprNode::getPreferredType()
{
   return TypeReqUInt;
}

//------------------------------------------------------------

U32 FloatUnaryExprNode::compile(CodeStream& codeStream, U32 ip, TypeReq type)
{
   ip = expr->compile(codeStream, ip, TypeReqFloat);
   codeStream.emit(OP_NEG);

   return codeStream.tell();
}

TypeReq FloatUnaryExprNode::getPreferredType()
{
   return TypeReqFloat;
}

//------------------------------------------------------------

U32 VarNode::compile(CodeStream& codeStream, U32 ip, TypeReq type)
{
   // if this has an arrayIndex...
   // OP_LOADIMMED_IDENT
   // varName
   // OP_ADVANCE_STR
   // evaluate arrayIndex TypeReqString
   // OP_REWIND_STR
   // OP_SETCURVAR_ARRAY
   // OP_LOADVAR (type)

   // else
   // OP_SETCURVAR
   // varName
   // OP_LOADVAR (type)

   if (type == TypeReqNone)
      return codeStream.tell();

   precompileIdent(varName);

   bool oldVariables = arrayIndex || varName[0] == '$';

   if (oldVariables)
   {
      codeStream.emit(arrayIndex ? OP_LOADIMMED_IDENT : OP_SETCURVAR);
      codeStream.emitSTE(varName);

      if (arrayIndex)
      {
         //codeStream.emit(OP_ADVANCE_STR);
         ip = arrayIndex->compile(codeStream, ip, TypeReqString);
         codeStream.emit(OP_REWIND_STR);
         codeStream.emit(OP_SETCURVAR_ARRAY);
         codeStream.emit(OP_POP_STK);
      }
      switch (type)
      {
      case TypeReqUInt:
         codeStream.emit(OP_LOADVAR_UINT);
         break;
      case TypeReqFloat:
         codeStream.emit(OP_LOADVAR_FLT);
         break;
      case TypeReqString:
         codeStream.emit(OP_LOADVAR_STR);
         break;
      case TypeReqNone:
         break;
      default:
         break;
      }
   }
   else
   {
      switch (type)
      {
      case TypeReqUInt:  codeStream.emit(OP_LOAD_LOCAL_VAR_UINT); break;
      case TypeReqFloat: codeStream.emit(OP_LOAD_LOCAL_VAR_FLT); break;
      default:           codeStream.emit(OP_LOAD_LOCAL_VAR_STR);
      }

      codeStream.emit(getFuncVars(dbgLineNumber)->lookup(varName, dbgLineNumber));
   }

   return codeStream.tell();
}

TypeReq VarNode::getPreferredType()
{
   bool oldVariables = arrayIndex || varName[0] == '$';
   return oldVariables ? TypeReqNone : getFuncVars(dbgLineNumber)->lookupType(varName, dbgLineNumber);
}

//------------------------------------------------------------

U32 IntNode::compile(CodeStream& codeStream, U32 ip, TypeReq type)
{
   if (type == TypeReqString)
      index = getCurrentStringTable()->addIntString(value);
   else if (type == TypeReqFloat)
      index = getCurrentFloatTable()->add(value);

   switch (type)
   {
   case TypeReqUInt:
      codeStream.emit(OP_LOADIMMED_UINT);
      codeStream.emit(value);
      break;
   case TypeReqString:
      codeStream.emit(OP_LOADIMMED_STR);
      codeStream.emit(index);
      break;
   case TypeReqFloat:
      codeStream.emit(OP_LOADIMMED_FLT);
      codeStream.emit(index);
      break;
   case TypeReqNone:
      break;
   }
   return codeStream.tell();
}

TypeReq IntNode::getPreferredType()
{
   return TypeReqUInt;
}

//------------------------------------------------------------

U32 FloatNode::compile(CodeStream& codeStream, U32 ip, TypeReq type)
{
   if (type == TypeReqString)
      index = getCurrentStringTable()->addFloatString(value);
   else if (type == TypeReqFloat)
      index = getCurrentFloatTable()->add(value);

   switch (type)
   {
   case TypeReqUInt:
      codeStream.emit(OP_LOADIMMED_UINT);
      codeStream.emit(U32(value));
      break;
   case TypeReqString:
      codeStream.emit(OP_LOADIMMED_STR);
      codeStream.emit(index);
      break;
   case TypeReqFloat:
      codeStream.emit(OP_LOADIMMED_FLT);
      codeStream.emit(index);
      break;
   case TypeReqNone:
      break;
   }
   return codeStream.tell();
}

TypeReq FloatNode::getPreferredType()
{
   return TypeReqFloat;
}

//------------------------------------------------------------

U32 StrConstNode::compile(CodeStream& codeStream, U32 ip, TypeReq type)
{
   // Early out for documentation block.
   if (doc)
   {
      index = getCurrentStringTable()->add(str, true, tag);
   }
   else if (type == TypeReqString)
   {
      index = getCurrentStringTable()->add(str, true, tag);
   }
   else if (type != TypeReqNone)
   {
      fVal = consoleStringToNumber(str, dbgFileName, dbgLineNumber);
      if (type == TypeReqFloat)
      {
         index = getCurrentFloatTable()->add(fVal);
      }
   }

   // If this is a DOCBLOCK, then process w/ appropriate op...
   if (doc)
   {
      codeStream.emit(OP_DOCBLOCK_STR);
      codeStream.emit(index);
      return ip;
   }

   // Otherwise, deal with it normally as a string literal case.
   switch (type)
   {
   case TypeReqString:
      codeStream.emit(tag ? OP_TAG_TO_STR : OP_LOADIMMED_STR);
      codeStream.emit(index);
      break;
   case TypeReqUInt:
      codeStream.emit(OP_LOADIMMED_UINT);
      codeStream.emit(U32(fVal));
      break;
   case TypeReqFloat:
      codeStream.emit(OP_LOADIMMED_FLT);
      codeStream.emit(index);
      break;
   case TypeReqNone:
      break;
   }
   return codeStream.tell();
}

TypeReq StrConstNode::getPreferredType()
{
   return TypeReqString;
}

//------------------------------------------------------------

U32 ConstantNode::compile(CodeStream& codeStream, U32 ip, TypeReq type)
{
   if (type == TypeReqString)
   {
      precompileIdent(value);
   }
   else if (type != TypeReqNone)
   {
      fVal = consoleStringToNumber(value, dbgFileName, dbgLineNumber);
      if (type == TypeReqFloat)
         index = getCurrentFloatTable()->add(fVal);
   }

   switch (type)
   {
   case TypeReqString:
      codeStream.emit(OP_LOADIMMED_IDENT);
      codeStream.emitSTE(value);
      break;
   case TypeReqUInt:
      codeStream.emit(OP_LOADIMMED_UINT);
      codeStream.emit(U32(fVal));
      break;
   case TypeReqFloat:
      codeStream.emit(OP_LOADIMMED_FLT);
      codeStream.emit(index);
      break;
   case TypeReqNone:
      break;
   }
   return ip;
}

TypeReq ConstantNode::getPreferredType()
{
   return TypeReqString;
}

//------------------------------------------------------------

U32 AssignExprNode::compile(CodeStream& codeStream, U32 ip, TypeReq type)
{
   subType = expr->getPreferredType();
   if (subType == TypeReqNone)
      subType = type;
   if (subType == TypeReqNone)
      subType = TypeReqString;

   // if it's an array expr, the formula is:
   // eval expr
   // (push and pop if it's TypeReqString) OP_ADVANCE_STR
   // OP_LOADIMMED_IDENT
   // varName
   // OP_ADVANCE_STR
   // eval array
   // OP_REWIND_STR
   // OP_SETCURVAR_ARRAY_CREATE
   // OP_TERMINATE_REWIND_STR
   // OP_SAVEVAR

   //else
   // eval expr
   // OP_SETCURVAR_CREATE
   // varname
   // OP_SAVEVAR

   precompileIdent(varName);

   ip = expr->compile(codeStream, ip, subType);

   bool oldVariables = arrayIndex || varName[0] == '$';

   if (oldVariables)
   {
      if (arrayIndex)
      {
         //if (subType == TypeReqString)
         //   codeStream.emit(OP_ADVANCE_STR);

         codeStream.emit(OP_LOADIMMED_IDENT);
         codeStream.emitSTE(varName);

         //codeStream.emit(OP_ADVANCE_STR);
         ip = arrayIndex->compile(codeStream, ip, TypeReqString);
         codeStream.emit(OP_REWIND_STR);
         codeStream.emit(OP_SETCURVAR_ARRAY_CREATE);
         if (type == TypeReqNone)
            codeStream.emit(OP_POP_STK);
      }
      else
      {
         codeStream.emit(OP_SETCURVAR_CREATE);
         codeStream.emitSTE(varName);
      }
      switch (subType)
      {
      case TypeReqString: codeStream.emit(OP_SAVEVAR_STR);  break;
      case TypeReqUInt:   codeStream.emit(OP_SAVEVAR_UINT); break;
      case TypeReqFloat:  codeStream.emit(OP_SAVEVAR_FLT);  break;
      default: break;
      }
   }
   else
   {
      switch (subType)
      {
      case TypeReqUInt:  codeStream.emit(OP_SAVE_LOCAL_VAR_UINT); break;
      case TypeReqFloat: codeStream.emit(OP_SAVE_LOCAL_VAR_FLT); break;
      default:           codeStream.emit(OP_SAVE_LOCAL_VAR_STR);
      }
      codeStream.emit(getFuncVars(dbgLineNumber)->assign(varName, subType == TypeReqNone ? TypeReqString : subType, dbgLineNumber));
   }

   if (type == TypeReqNone)
      codeStream.emit(OP_POP_STK);
   return ip;
}

TypeReq AssignExprNode::getPreferredType()
{
   return expr->getPreferredType();
}

//------------------------------------------------------------

static void getAssignOpTypeOp(S32 op, TypeReq& type, U32& operand)
{
   switch (op)
   {
   case opPLUSPLUS:
      TORQUE_CASE_FALLTHROUGH;
   case '+':
      type = TypeReqFloat;
      operand = OP_ADD;
      break;
   case opMINUSMINUS:
      TORQUE_CASE_FALLTHROUGH;
   case '-':
      type = TypeReqFloat;
      operand = OP_SUB;
      break;
   case '*':
      type = TypeReqFloat;
      operand = OP_MUL;
      break;
   case '/':
      type = TypeReqFloat;
      operand = OP_DIV;
      break;
   case '%':
      type = TypeReqUInt;
      operand = OP_MOD;
      break;
   case '&':
      type = TypeReqUInt;
      operand = OP_BITAND;
      break;
   case '^':
      type = TypeReqUInt;
      operand = OP_XOR;
      break;
   case '|':
      type = TypeReqUInt;
      operand = OP_BITOR;
      break;
   case opSHL:
      type = TypeReqUInt;
      operand = OP_SHL;
      break;
   case opSHR:
      type = TypeReqUInt;
      operand = OP_SHR;
      break;
   default:
      AssertFatal(false, "Invalid opcode on operation expression");
   }
}

U32 AssignOpExprNode::compile(CodeStream& codeStream, U32 ip, TypeReq type)
{

   // goes like this...
   // eval expr as float or int
   // if there's an arrayIndex

   // OP_LOADIMMED_IDENT
   // varName
   // OP_ADVANCE_STR
   // eval arrayIndex stringwise
   // OP_REWIND_STR
   // OP_SETCURVAR_ARRAY_CREATE

   // else
   // OP_SETCURVAR_CREATE
   // varName

   // OP_LOADVAR_FLT or UINT
   // operand
   // OP_SAVEVAR_FLT or UINT

   // conversion OP if necessary.
   getAssignOpTypeOp(op, subType, operand);
   precompileIdent(varName);

   bool oldVariables = arrayIndex || varName[0] == '$';

   if (op == opPLUSPLUS && !oldVariables && type == TypeReqNone)
   {
      const S32 varIdx = getFuncVars(dbgLineNumber)->assign(varName, TypeReqFloat, dbgLineNumber);

      codeStream.emit(OP_INC);
      codeStream.emit(varIdx);
   }
   else
   {
      ip = expr->compile(codeStream, ip, subType);

      if (oldVariables)
      {
         if (!arrayIndex)
         {
            codeStream.emit(OP_SETCURVAR_CREATE);
            codeStream.emitSTE(varName);
         }
         else
         {
            codeStream.emit(OP_LOADIMMED_IDENT);
            codeStream.emitSTE(varName);

            //codeStream.emit(OP_ADVANCE_STR);
            ip = arrayIndex->compile(codeStream, ip, TypeReqString);
            codeStream.emit(OP_REWIND_STR);
            codeStream.emit(OP_SETCURVAR_ARRAY_CREATE);
            if (type == TypeReqNone)
               codeStream.emit(OP_POP_STK);
         }
         codeStream.emit((subType == TypeReqFloat) ? OP_LOADVAR_FLT : OP_LOADVAR_UINT);
         codeStream.emit(operand);
         codeStream.emit((subType == TypeReqFloat) ? OP_SAVEVAR_FLT : OP_SAVEVAR_UINT);
      }
      else
      {
         const bool isFloat = subType == TypeReqFloat;
         const S32 varIdx = getFuncVars(dbgLineNumber)->assign(varName, subType == TypeReqNone ? TypeReqString : subType, dbgLineNumber);

         codeStream.emit(isFloat ? OP_LOAD_LOCAL_VAR_FLT : OP_LOAD_LOCAL_VAR_UINT);
         codeStream.emit(varIdx);
         codeStream.emit(operand);
         codeStream.emit(isFloat ? OP_SAVE_LOCAL_VAR_FLT : OP_SAVE_LOCAL_VAR_UINT);
         codeStream.emit(varIdx);
      }

      if (type == TypeReqNone)
         codeStream.emit(OP_POP_STK);
   }
   return codeStream.tell();
}

TypeReq AssignOpExprNode::getPreferredType()
{
   getAssignOpTypeOp(op, subType, operand);
   return subType;
}

//------------------------------------------------------------

U32 TTagSetStmtNode::compileStmt(CodeStream&, U32 ip)
{
   return ip;
}

//------------------------------------------------------------

U32 TTagDerefNode::compile(CodeStream&, U32 ip, TypeReq)
{
   return ip;
}

TypeReq TTagDerefNode::getPreferredType()
{
   return TypeReqNone;
}

//------------------------------------------------------------

U32 TTagExprNode::compile(CodeStream&, U32 ip, TypeReq)
{
   return ip;
}

TypeReq TTagExprNode::getPreferredType()
{
   return TypeReqNone;
}

//------------------------------------------------------------

U32 FuncCallExprNode::compile(CodeStream& codeStream, U32 ip, TypeReq type)
{
   // OP_PUSH_FRAME
   // arg OP_PUSH arg OP_PUSH arg OP_PUSH
   // eval all the args, then call the function.

   // OP_CALLFUNC
   // function
   // namespace
   // isDot

   precompileIdent(funcName);
   precompileIdent(nameSpace);

   S32 count = 0;
   for (ExprNode* walk = args; walk; walk = static_cast<ExprNode*>(walk->getNext()))
      count++;

   codeStream.emit(OP_PUSH_FRAME);
   codeStream.emit(count);

   for (ExprNode* walk = args; walk; walk = static_cast<ExprNode*>(walk->getNext()))
   {
      TypeReq walkType = walk->getPreferredType();
      if (walkType == TypeReqNone)
         walkType = TypeReqString;

      ip = walk->compile(codeStream, ip, walkType);
      codeStream.emit(OP_PUSH);
   }

   codeStream.emit(OP_CALLFUNC);
   codeStream.emitSTE(funcName);
   codeStream.emitSTE(nameSpace);
   codeStream.emit(callType);

   if (type == TypeReqNone)
      codeStream.emit(OP_POP_STK);

   return codeStream.tell();
}

TypeReq FuncCallExprNode::getPreferredType()
{
   return TypeReqString;
}

//------------------------------------------------------------

U32 AssertCallExprNode::compile(CodeStream& codeStream, U32 ip, TypeReq type)
{
#ifdef TORQUE_ENABLE_SCRIPTASSERTS

   messageIndex = getCurrentStringTable()->add(message, true, false);

   ip = testExpr->compile(codeStream, ip, TypeReqUInt);
   codeStream.emit(OP_ASSERT);
   codeStream.emit(messageIndex);

#endif

   return codeStream.tell();
}

TypeReq AssertCallExprNode::getPreferredType()
{
   return TypeReqNone;
}

//------------------------------------------------------------

U32 SlotAccessNode::compile(CodeStream& codeStream, U32 ip, TypeReq type)
{
   if (type == TypeReqNone)
      return ip;

   precompileIdent(slotName);

   if (arrayExpr)
   {
      ip = arrayExpr->compile(codeStream, ip, TypeReqString);
   }
   ip = objectExpr->compile(codeStream, ip, TypeReqString);
   codeStream.emit(OP_SETCUROBJECT);

   codeStream.emit(OP_SETCURFIELD);
   codeStream.emitSTE(slotName);

   codeStream.emit(OP_POP_STK);

   if (arrayExpr)
   {
      codeStream.emit(OP_SETCURFIELD_ARRAY);
      codeStream.emit(OP_POP_STK);
   }

   switch (type)
   {
   case TypeReqUInt:
      codeStream.emit(OP_LOADFIELD_UINT);
      break;
   case TypeReqFloat:
      codeStream.emit(OP_LOADFIELD_FLT);
      break;
   case TypeReqString:
      codeStream.emit(OP_LOADFIELD_STR);
      break;
   case TypeReqNone:
      break;
   }
   return codeStream.tell();
}

TypeReq SlotAccessNode::getPreferredType()
{
   return TypeReqNone;
}

//-----------------------------------------------------------------------------

U32 InternalSlotAccessNode::compile(CodeStream& codeStream, U32 ip, TypeReq type)
{
   if (type == TypeReqNone)
      return ip;

   ip = objectExpr->compile(codeStream, ip, TypeReqString);
   codeStream.emit(OP_SETCUROBJECT);

   // we pop the stack as we will override the current object with the internal object
   codeStream.emit(OP_POP_STK);

   ip = slotExpr->compile(codeStream, ip, TypeReqString);
   codeStream.emit(OP_SETCUROBJECT_INTERNAL);
   codeStream.emit(recurse);

   return codeStream.tell();
}

TypeReq InternalSlotAccessNode::getPreferredType()
{
   return TypeReqUInt;
}

//-----------------------------------------------------------------------------

U32 SlotAssignNode::compile(CodeStream& codeStream, U32 ip, TypeReq type)
{
   precompileIdent(slotName);

   ip = valueExpr->compile(codeStream, ip, TypeReqString);
   if (arrayExpr)
   {
      ip = arrayExpr->compile(codeStream, ip, TypeReqString);
   }
   if (objectExpr)
   {
      ip = objectExpr->compile(codeStream, ip, TypeReqString);
      codeStream.emit(OP_SETCUROBJECT);
   }
   else
      codeStream.emit(OP_SETCUROBJECT_NEW);
   codeStream.emit(OP_SETCURFIELD);
   codeStream.emitSTE(slotName);

   if (objectExpr)
   {
      // Don't pop unless we are assigning a field to an object
      // (For initializer fields, we don't wanna pop)
      codeStream.emit(OP_POP_STK);
   }

   if (arrayExpr)
   {
      codeStream.emit(OP_SETCURFIELD_ARRAY);
      codeStream.emit(OP_POP_STK);
   }

   codeStream.emit(OP_SAVEFIELD_STR);

   if (typeID != -1)
   {
      codeStream.emit(OP_SETCURFIELD_TYPE);
      codeStream.emit(typeID);
   }

   if (type == TypeReqNone)
      codeStream.emit(OP_POP_STK);

   return codeStream.tell();
}

TypeReq SlotAssignNode::getPreferredType()
{
   return TypeReqString;
}

//------------------------------------------------------------

U32 SlotAssignOpNode::compile(CodeStream& codeStream, U32 ip, TypeReq type)
{
   // first eval the expression as its type

   // if it's an array:
   // eval array
   // OP_ADVANCE_STR
   // evaluate object expr
   // OP_SETCUROBJECT
   // OP_SETCURFIELD
   // fieldName
   // OP_TERMINATE_REWIND_STR
   // OP_SETCURFIELDARRAY

   // else
   // evaluate object expr
   // OP_SETCUROBJECT
   // OP_SETCURFIELD
   // fieldName

   // OP_LOADFIELD of appropriate type
   // operand
   // OP_SAVEFIELD of appropriate type
   // convert to return type if necessary.

   getAssignOpTypeOp(op, subType, operand);
   precompileIdent(slotName);

   ip = valueExpr->compile(codeStream, ip, subType);
   if (arrayExpr)
   {
      ip = arrayExpr->compile(codeStream, ip, TypeReqString);
   }
   ip = objectExpr->compile(codeStream, ip, TypeReqString);
   codeStream.emit(OP_SETCUROBJECT);
   codeStream.emit(OP_SETCURFIELD);
   codeStream.emitSTE(slotName);

   codeStream.emit(OP_POP_STK);

   if (arrayExpr)
   {
      codeStream.emit(OP_SETCURFIELD_ARRAY);
      if (subType == TypeReqNone || type == TypeReqNone)
         codeStream.emit(OP_POP_STK);
   }
   codeStream.emit((subType == TypeReqFloat) ? OP_LOADFIELD_FLT : OP_LOADFIELD_UINT);
   codeStream.emit(operand);
   codeStream.emit((subType == TypeReqFloat) ? OP_SAVEFIELD_FLT : OP_SAVEFIELD_UINT);
   if (type == TypeReqNone)
      codeStream.emit(OP_POP_STK);
   return codeStream.tell();
}

TypeReq SlotAssignOpNode::getPreferredType()
{
   getAssignOpTypeOp(op, subType, operand);
   return subType;
}

//------------------------------------------------------------

U32 ObjectDeclNode::compileSubObject(CodeStream& codeStream, U32 ip, bool root)
{
   // goes

   // OP_PUSHFRAME 1
   // name expr
   // OP_PUSH 1
   // args... PUSH
   // OP_CREATE_OBJECT 1
   // parentObject 1
   // isDatablock 1
   // internalName 1
   // isSingleton 1
   // lineNumber 1
   // fail point 1

   // for each field, eval
   // OP_ADD_OBJECT (to UINT[0]) 1
   // root? 1

   // add all the sub objects.
   // OP_END_OBJECT 1
   // root? 1
   // To fix the stack issue [7/9/2007 Black]
   // OP_FINISH_OBJECT <-- fail point jumps to this opcode

   S32 count = 2; // 2 OP_PUSH's
   for (ExprNode* exprWalk = argList; exprWalk; exprWalk = (ExprNode*)exprWalk->getNext())
      count++;

   codeStream.emit(OP_PUSH_FRAME);
   codeStream.emit(count);

   ip = classNameExpr->compile(codeStream, ip, TypeReqString);
   codeStream.emit(OP_PUSH);

   ip = objectNameExpr->compile(codeStream, ip, TypeReqString);
   codeStream.emit(OP_PUSH);
   for (ExprNode* exprWalk = argList; exprWalk; exprWalk = (ExprNode*)exprWalk->getNext())
   {
      TypeReq walkType = exprWalk->getPreferredType();
      if (walkType == TypeReqNone) walkType = TypeReqString;
      ip = exprWalk->compile(codeStream, ip, walkType);
      codeStream.emit(OP_PUSH);
   }
   codeStream.emit(OP_CREATE_OBJECT);
   codeStream.emitSTE(parentObject);

   codeStream.emit(isDatablock);
   codeStream.emit(isClassNameInternal);
   codeStream.emit(isSingleton);
   codeStream.emit(dbgLineNumber);
   const U32 failIp = codeStream.emit(0);
   for (SlotAssignNode* slotWalk = slotDecls; slotWalk; slotWalk = (SlotAssignNode*)slotWalk->getNext())
      ip = slotWalk->compile(codeStream, ip, TypeReqNone);
   codeStream.emit(OP_ADD_OBJECT);
   codeStream.emit(root);
   for (ObjectDeclNode* objectWalk = subObjects; objectWalk; objectWalk = (ObjectDeclNode*)objectWalk->getNext())
      ip = objectWalk->compileSubObject(codeStream, ip, false);
   codeStream.emit(OP_END_OBJECT);
   codeStream.emit(root || isDatablock);
   // Added to fix the object creation issue [7/9/2007 Black]
   failOffset = codeStream.emit(OP_FINISH_OBJECT);

   codeStream.patch(failIp, failOffset);

   return codeStream.tell();
}

U32 ObjectDeclNode::compile(CodeStream& codeStream, U32 ip, TypeReq type)
{
   // root object decl does:

   // push 0 onto the UINT stack OP_LOADIMMED_UINT
   // precompiles the subObject(true)
   // UINT stack now has object id
   // type conv to type

   codeStream.emit(OP_LOADIMMED_UINT);
   codeStream.emit(0);
   ip = compileSubObject(codeStream, ip, true);

   if (type == TypeReqNone)
      codeStream.emit(OP_POP_STK);

   return codeStream.tell();
}

TypeReq ObjectDeclNode::getPreferredType()
{
   return TypeReqUInt;
}

//------------------------------------------------------------

U32 FunctionDeclStmtNode::compileStmt(CodeStream& codeStream, U32 ip)
{
   // OP_FUNC_DECL
   // func name
   // namespace
   // package
   // hasBody?
   // func end ip
   // argc
   // ident array[argc]
   // code
   // OP_RETURN_VOID
   setCurrentStringTable(&getFunctionStringTable());
   setCurrentFloatTable(&getFunctionFloatTable());

   FuncVars vars;
   gFuncVars = &vars;

   argc = 0;
   for (VarNode* walk = args; walk; walk = (VarNode*)((StmtNode*)walk)->getNext())
   {
      precompileIdent(walk->varName);
      getFuncVars(dbgLineNumber)->assign(walk->varName, TypeReqNone, dbgLineNumber);
      argc++;
   }

   CodeBlock::smInFunction = true;

   precompileIdent(fnName);
   precompileIdent(nameSpace);
   precompileIdent(package);

   CodeBlock::smInFunction = false;

   codeStream.emit(OP_FUNC_DECL);
   codeStream.emitSTE(fnName);
   codeStream.emitSTE(nameSpace);
   codeStream.emitSTE(package);

   codeStream.emit(U32(bool(stmts != NULL) ? 1 : 0) + U32(dbgLineNumber << 1));
   const U32 endIp = codeStream.emit(0);
   codeStream.emit(argc);
   const U32 localNumVarsIP = codeStream.emit(0);
   for (VarNode* walk = args; walk; walk = (VarNode*)((StmtNode*)walk)->getNext())
   {
      StringTableEntry name = walk->varName;
      codeStream.emit(getFuncVars(dbgLineNumber)->lookup(name, dbgLineNumber));
   }
   CodeBlock::smInFunction = true;
   ip = compileBlock(stmts, codeStream, ip);

   // Add break so breakpoint can be set at closing brace or
   // in empty function.
   addBreakLine(codeStream);

   CodeBlock::smInFunction = false;
   codeStream.emit(OP_RETURN_VOID);

   codeStream.patch(localNumVarsIP, getFuncVars(dbgLineNumber)->count());
   codeStream.patch(endIp, codeStream.tell());

   setCurrentStringTable(&getGlobalStringTable());
   setCurrentFloatTable(&getGlobalFloatTable());

   // map local variables to registers for this function.
   // Note we have to map these in order because the table itself is ordered by the register id.
   CompilerLocalVariableToRegisterMappingTable* tbl = &getFunctionVariableMappingTable();
   for (S32 i = 0; i < gFuncVars->variableNameMap.size(); ++i)
   {
      StringTableEntry varName = gFuncVars->variableNameMap[i];
      tbl->add(fnName, nameSpace, varName);
   }

   gFuncVars = gIsEvalCompile ? &gEvalFuncVars : &gGlobalScopeFuncVars;

   return ip;
}
