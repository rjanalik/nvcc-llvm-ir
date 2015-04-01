#include <llvm/Module.h>
#include <llvm/LLVMContext.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/InstrTypes.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

#include <cstdio>
#include <dlfcn.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <streambuf>
#include <string>

using namespace llvm;
using namespace std;

static void modifyModule(Module* module)
{
	if (!module) return;

	cout << endl
	     << "-----------------------" << endl
		 << "|--- pass begin -------|" << endl
		 << "-----------------------" << endl << endl;

	cout << "Module " << endl;

	// Add suffix to function name, for example.
	for (Module::iterator f = module->begin(), fe = module->end(); f != fe; f++)
	{
		cout << "Function " << f->getNameStr() << endl;

		for (Function::iterator bb = f->begin(), bbe = f->end(); bb != bbe; bb++)
		{
			cout << "BasicBlock " << bb->getNameStr() << " instructions " << bb->size() << endl;

			for (BasicBlock::iterator i = bb->begin(), ie = bb->end(); i != ie; i++)
			{
				unsigned int opcode = i->getOpcode();
				unsigned int numOperands = i->getNumOperands();

				cout << "Instruction opcode " << opcode << " opcode name " << i->getOpcodeName()
				     << " operands " << numOperands;
				for (unsigned int opIndex = 0; opIndex < numOperands; opIndex++)
				{
					Value* operand = i->getOperand(opIndex);
					cout << " " << operand->getNameStr();
				}
			    cout << endl;
				outs() << *i;
				cout << endl;

				//
				Instruction* newInstruction = NULL;

				switch (opcode)
				{
					case 9:		// add
						newInstruction = BinaryOperator::Create(Instruction::Mul, i->getOperand(0), i->getOperand(1));
						break;
					case 49:	// call
						cout << "call instruction, operand 2: " << i->getOperand(1) << endl;
						break;
				}

				if (newInstruction != NULL)
				{
					cout << "newInstruction opcode " << newInstruction->getOpcode() << " opcode name " << newInstruction->getOpcodeName()
							 << " operands " << newInstruction->getNumOperands();
					for (unsigned int opIndex = 0; opIndex < newInstruction->getNumOperands(); opIndex++)
					{
						cout << " " << newInstruction->getOperand(opIndex)->getNameStr();
					}
					cout << endl;
					outs() << *newInstruction;
					cout << endl;

					//BasicBlock::iterator ii(i);
					//ReplaceInstWithInst(i->getParent()->getInstList(), ii, newInstruction);
					//ReplaceInstWithInst(i, newInstruction);
					ReplaceInstWithInst(i->getParent()->getInstList(), i, newInstruction);
				}
			}
		}
	}
	
	cout << endl
	     << "-----------------------" << endl
		 << "|--- pass end --------|" << endl
		 << "-----------------------" << endl << endl;

	//cout << module.getFunctionList() << endl;
}

int main(int argc, char* argv[])
{
	if (argc != 2)
	{
		cout << "Usage: " << argv[0] << " <filename>" << endl;
		return 0;
	}

	ifstream inputstr(argv[1], ios::in | ios::binary);
	string input;
	input.assign(istreambuf_iterator<char>(inputstr), istreambuf_iterator<char>());
	
	string err;
	LLVMContext &context = getGlobalContext();
	MemoryBuffer* mbinput = MemoryBuffer::getMemBuffer(input);
	Module* module = ParseBitcodeFile(mbinput, context, &err);
	if (!module)
		cout << "Error parsing module bitcode : " << err;

	cout << "--------- before -----------" << endl;
	outs() << *module;
	cout << "--------- before end -------" << endl;

	modifyModule(module);

	cout << "--------- after ------------" << endl;
	outs() << *module;
	cout << "--------- after end --------" << endl;

	// Save module into bitcode.
	SmallVector<char, 128> bitcode;
	raw_svector_ostream outputStream(bitcode);
	WriteBitcodeToFile(module, outputStream);
	outputStream.flush();
	
	// Store LLVM IR bitcode into temp file.
	input = "";
	input.reserve(bitcode.size());
	input.assign(bitcode.data(), bitcode.data() + bitcode.size());
	ofstream tmpstream(argv[1], ios::out | ios::binary);
	tmpstream << input;
	tmpstream.close();
	
	return 0;
}

