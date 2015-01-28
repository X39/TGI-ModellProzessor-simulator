#include <iostream>
#include <stdio.h>
#include <vector>
#include <string>
#include <Windows.h>

#define CHECK_INSTRUCTION_NAME(BUFFER, INSTRUCTION) ((INSTRUCTION)[0] == (BUFFER)[0] && (INSTRUCTION)[1] == (BUFFER)[1] && (INSTRUCTION)[2] == (BUFFER)[2])

using namespace std;

enum Instructions
{
	NOP = 0,
	LDA_VA,
	LDA_ST,
	STA,
	ADD_VA,
	ADD_ST,
	SUB_VA,
	SUB_ST,
	JMP,
	BRZ,
	BRC,
	BRN
};

string instructionEnumToString(int i)
{
	if(i > Instructions::BRN || i < 0)
		return "NA";
	string translate[] = {
							string("NOP"),
							string("LDA"),
							string("LDA"),
							string("STA"),
							string("ADD"),
							string("ADD"),
							string("SUB"),
							string("SUB"),
							string("JMP"),
							string("BRZ"),
							string("BRC"),
							string("BRN")
						 };
	return translate[i];
}

typedef struct stQualifiedInstruction
{
	Instructions instrcution;
	int value;
	long long line;
}QINSTR;
typedef struct stLabel
{
	string name;
	unsigned int line;
}LABEL;

vector<QINSTR*> instructionList;
vector<LABEL*> labelList;

bool convAsciiCharToDouble(const char* s, double* out)
{
	long int i = 0;
	const char* numbers = NULL;
	int numberLength = -1;
	int dotSpot = -1;
	if(s[0] != 0x00 && s[1] != 0x00 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
	{//HEX
		//Do we got faked here? Validating that AT LEAST 1 HEXA digit is used after the 0x
		for(int index = 2;; index++)
		{
			const char c = s[index];
			if(c == 0x00)
				break;
			if((c >= 48 && c <= 57) || (c >= 65 && c <= 70) || (c >= 97 && c <= 102))
			{
				numbers = s + index;
				break;
			}
		}
		//Got no HEXA digit? Return false
		if(numbers == NULL)
			return false;
		//Count HEXA digits
		for(int index = 0;; index++)
		{
			const char c = numbers[index];
			if(!((c >= 48 && c <= 57) || (c >= 65 && c <= 70) || (c >= 97 && c <= 102)))
				break;
			numberLength++;
		}
		//Convert HEXA digits to propper integer
		for(int index = numberLength; index >= 0; index--)
		{

			int currentHandleNumber;
			if(((int)numbers[index]) > '9')
			{
				if(((int)numbers[index]) > 'F')
					currentHandleNumber = ((int)numbers[index]) - 'a' + 10;
				else
					currentHandleNumber = ((int)numbers[index]) - 'A' + 10;
			}
			else
			{
				currentHandleNumber = ((int)numbers[index]) - '0';
			}

			i += currentHandleNumber << ((int)(numberLength - index) * 4);
		}
	}
	else
	{//DOUBLE
		//Search for first existing digit (input "qwt 123" will put a pointer to the '1' into numbers)
		for(int index = 0;; index++)
		{
			const char c = s[index];
			if(c == 0x00)
				break;
			if(c >= 48 && c <= 57)
			{
				//found a digit, set pointer & break out of loop
				numbers = s + index;
				break;
			}
		}
		//No digit found? Return false
		if(numbers == NULL)
			return false;
		//Count digits (ignoring the dot)
		for(int index = 0;; index++)
		{
			const char c = numbers[index];
			if((c < 48 || c > 57) && c != 46)
				break;
			numberLength++;
		}
		//Convert digits into proper scalar numbers 
		for(int index = numberLength; index >= 0; index--)
		{
			int currentHandleNumber = ((int)numbers[index]) - 48;
			if(currentHandleNumber < 0)
			{
				//dot located, we got a float number here!
				dotSpot = numberLength - index;
				numberLength--;
			}
			else
			{
				//use 10^I * X to get the final result
				i += currentHandleNumber * pow((double)10, (int)(numberLength - index));
			}
		}
		//If we had a dot, we will now shift our results comma
		if(dotSpot != -1)
			*out = i / pow((double)10, (int)(dotSpot));
		else
			*out = i;
	}
	return true;
}

void cleanup(void)
{
	for(int i = 0; i < instructionList.size(); i++)
		delete instructionList[i];
	instructionList.clear();
	for(int i = 0; i < labelList.size(); i++)
		delete labelList[i];
	labelList.clear();
}


void main(int argc, char *argv[])
{
	char buffer[256];
	long long linecount = 0;
	vector<QINSTR*> branchInstructionsToLink;
	vector<string> branchinstructionsToLink_LabelNames;
	bool perLineExecute = false;
	double stack[20];
	double currentValue;
	struct{bool zero; bool carry; bool negation;} stateRegister;
	stateRegister.carry = false;
	stateRegister.negation = false;
	stateRegister.zero = false;
	cout << "USABLE REGISTERS: 10 - 20" << endl;
#pragma region ParseAssemblerProgram
	while(true)
	{
		bool labelAdded = false;
		cin.getline(buffer, 255);
		for (int i = 0; buffer[i] != 0x00; i++) buffer[i] = toupper(buffer[i]);
		if(strlen(buffer) == 0)
			continue;
		if(strlen(buffer) < 5)
		{
			cout << "Invalid input! Please provide at least 5 chars ..." << endl;
			continue;
		}
		char* firstSpaceOccurrence = strchr(buffer, ' ');
		if(firstSpaceOccurrence - buffer < 3) //Buffer ~ "BB DONTCAREFROMHERE"
		{
			cout << "Invalid input! You need to enter at least a 3char command/label!" << endl;
			continue;
		}
		if((firstSpaceOccurrence - 1)[0] == ':')
		{
			(firstSpaceOccurrence)[0] = 0x00;
			LABEL* l = new LABEL();
			l->name = string(buffer);
			l->line = linecount;
			labelList.push_back(l);
			labelAdded = true;
			(firstSpaceOccurrence)[0] = ' ';
			if(strchr(firstSpaceOccurrence + 1, ' ') != 0x00)
				firstSpaceOccurrence = strchr(firstSpaceOccurrence + 1, ' ');
			else
				firstSpaceOccurrence = strchr(firstSpaceOccurrence + 1, 0x00);
		}
		if(firstSpaceOccurrence - buffer < 3) //Buffer ~ "BB DONTCAREFROMHERE"
		{
			cout << "Invalid input! You need to enter at least a 3char command/label!" << endl;
			continue;
		}
		if(CHECK_INSTRUCTION_NAME(firstSpaceOccurrence - 3, "NOP"))
		{
			QINSTR* instr = new QINSTR();
			instr->instrcution = Instructions::NOP;
			instr->value = 0;
			instr->line = linecount;
			instructionList.push_back(instr);
		}
		else if(CHECK_INSTRUCTION_NAME(firstSpaceOccurrence - 3, "LDA"))
		{
			if(((firstSpaceOccurrence + 1)[0] > '9' || (firstSpaceOccurrence + 1)[0] < '0') && (firstSpaceOccurrence + 1)[0] != '(' && (firstSpaceOccurrence + 1)[0] != '#')
			{
				cout << "Invalid input! Instruction parameter is not numeric" << endl;
				continue;
			}
			QINSTR* instr = new QINSTR();
			instr->line = linecount;
			double d;
			if(!convAsciiCharToDouble((firstSpaceOccurrence + 1), &d))
			{
				cout << "Invalid input! Cannot parse value '" << (firstSpaceOccurrence + 1) << "'" << endl;
				delete instr;
				continue;
			}
			instr->value = d;
			if((firstSpaceOccurrence + 1)[0] == '(')
				instr->instrcution = Instructions::LDA_ST;
			else
				instr->instrcution = Instructions::LDA_VA;
			instructionList.push_back(instr);
		}
		else if(CHECK_INSTRUCTION_NAME(firstSpaceOccurrence - 3, "STA"))
		{
			if((firstSpaceOccurrence + 1)[0] > '9' || (firstSpaceOccurrence + 1)[0] < '0')
			{
				cout << "Invalid input! Instruction parameter is not numeric" << endl;
				continue;
			}
			
			QINSTR* instr = new QINSTR();
			instr->instrcution = Instructions::STA;
			instr->line = linecount;
			double d;
			if(!convAsciiCharToDouble((firstSpaceOccurrence + 1), &d))
			{
				cout << "Invalid input! Cannot parse value '" << (firstSpaceOccurrence + 1) << "'" << endl;
				delete instr;
				continue;
			}
			instr->value = d;
			instructionList.push_back(instr);
		}
		else if(CHECK_INSTRUCTION_NAME(firstSpaceOccurrence - 3, "ADD"))
		{
			if(((firstSpaceOccurrence + 1)[0] > '9' || (firstSpaceOccurrence + 1)[0] < '0') && (firstSpaceOccurrence + 1)[0] != '(' && (firstSpaceOccurrence + 1)[0] != '#')
			{
				cout << "Invalid input! Instruction parameter is not numeric" << endl;
				continue;
			}
			QINSTR* instr = new QINSTR();
			double d;
			if(!convAsciiCharToDouble((firstSpaceOccurrence + 1), &d))
			{
				cout << "Invalid input! Cannot parse value '" << (firstSpaceOccurrence + 1) << "'" << endl;
				delete instr;
				continue;
			}
			instr->line = linecount;
			instr->value = d;
			if((firstSpaceOccurrence + 1)[0] == '(')
				instr->instrcution = Instructions::ADD_ST;
			else
				instr->instrcution = Instructions::ADD_VA;
			instructionList.push_back(instr);
		}
		else if(CHECK_INSTRUCTION_NAME(firstSpaceOccurrence - 3, "SUB"))
		{
			if(((firstSpaceOccurrence + 1)[0] > '9' || (firstSpaceOccurrence + 1)[0] < '0') && (firstSpaceOccurrence + 1)[0] != '(' && (firstSpaceOccurrence + 1)[0] != '#')
			{
				cout << "Invalid input! Instruction parameter is not numeric" << endl;
				continue;
			}
			QINSTR* instr = new QINSTR();
			instr->line = linecount;
			double d;
			if(!convAsciiCharToDouble((firstSpaceOccurrence + 1), &d))
			{
				cout << "Invalid input! Cannot parse value '" << (firstSpaceOccurrence + 1) << "'" << endl;
				delete instr;
				continue;
			}
			instr->value = d;
			if((firstSpaceOccurrence + 1)[0] == '(')
				instr->instrcution = Instructions::SUB_ST;
			else
				instr->instrcution = Instructions::SUB_VA;
			instructionList.push_back(instr);
		}
		else if(CHECK_INSTRUCTION_NAME(firstSpaceOccurrence - 3, "JMP"))
		{
			QINSTR* instr = new QINSTR();
			instr->line = linecount;
			instr->instrcution = Instructions::JMP;
			if(((firstSpaceOccurrence + 1)[0] > '9' || (firstSpaceOccurrence + 1)[0] < '0') && (firstSpaceOccurrence + 1)[0] != '-')
			{
				branchInstructionsToLink.push_back(instr);
				branchinstructionsToLink_LabelNames.push_back(string(firstSpaceOccurrence + 1));
				instr->value = 0;
			}
			else
			{
				double d;
				if(!convAsciiCharToDouble((firstSpaceOccurrence + 1), &d))
				{
					cout << "Invalid input! Cannot parse value '" << (firstSpaceOccurrence + 1) << "'" << endl;
					delete instr;
					continue;
				}
				instr->value = d;
			}
			instructionList.push_back(instr);
		}
		else if(CHECK_INSTRUCTION_NAME(firstSpaceOccurrence - 3, "BRZ"))
		{
			QINSTR* instr = new QINSTR();
			instr->line = linecount;
			instr->instrcution = Instructions::BRZ;
			if(((firstSpaceOccurrence + 1)[0] > '9' || (firstSpaceOccurrence + 1)[0] < '0') && (firstSpaceOccurrence + 1)[0] != '-' && (firstSpaceOccurrence + 1)[0] != '#')
			{
				branchInstructionsToLink.push_back(instr);
				branchinstructionsToLink_LabelNames.push_back(string(firstSpaceOccurrence + 1));
				instr->value = 0;
			}
			else
			{
				double d;
				if(!convAsciiCharToDouble((firstSpaceOccurrence + 1), &d))
				{
					cout << "Invalid input! Cannot parse value '" << (firstSpaceOccurrence + 1) << "'" << endl;
					delete instr;
					continue;
				}
				instr->value = d;
			}
			instructionList.push_back(instr);
		}
		else if(CHECK_INSTRUCTION_NAME(firstSpaceOccurrence - 3, "BRC"))
		{
			QINSTR* instr = new QINSTR();
			instr->line = linecount;
			instr->instrcution = Instructions::BRC;
			if(((firstSpaceOccurrence + 1)[0] > '9' || (firstSpaceOccurrence + 1)[0] < '0') && (firstSpaceOccurrence + 1)[0] != '-' && (firstSpaceOccurrence + 1)[0] != '#')
			{
				branchInstructionsToLink.push_back(instr);
				branchinstructionsToLink_LabelNames.push_back(string(firstSpaceOccurrence + 1));
				instr->value = 0;
			}
			else
			{
				double d;
				if(!convAsciiCharToDouble((firstSpaceOccurrence + 1), &d))
				{
					cout << "Invalid input! Cannot parse value '" << (firstSpaceOccurrence + 1) << "'" << endl;
					delete instr;
					continue;
				}
				instr->value = d;
			}
			instructionList.push_back(instr);
		}
		else if(CHECK_INSTRUCTION_NAME(firstSpaceOccurrence - 3, "BRN"))
		{
			QINSTR* instr = new QINSTR();
			instr->instrcution = Instructions::BRN;
			instr->line = linecount;
			if(((firstSpaceOccurrence + 1)[0] > '9' || (firstSpaceOccurrence + 1)[0] < '0') && (firstSpaceOccurrence + 1)[0] != '-' && (firstSpaceOccurrence + 1)[0] != '#')
			{
				branchInstructionsToLink.push_back(instr);
				branchinstructionsToLink_LabelNames.push_back(string(firstSpaceOccurrence + 1));
				instr->value = 0;
			}
			else
			{
				double d;
				if(!convAsciiCharToDouble((firstSpaceOccurrence + 1), &d))
				{
					cout << "Invalid input! Cannot parse value '" << (firstSpaceOccurrence + 1) << "'" << endl;
					delete instr;
					continue;
				}
				instr->value = d;
			}
			instructionList.push_back(instr);
		}
		else if(CHECK_INSTRUCTION_NAME(firstSpaceOccurrence - 3, "RUN"))
		{//No actual instruction ... but we need to tell this routine somehow to break out and execute ^^
			double d;
			if(!convAsciiCharToDouble((firstSpaceOccurrence + 1), &d))
			{
				cout << "Invalid input! Cannot parse value '" << (firstSpaceOccurrence + 1) << "'" << endl;
				continue;
			}
			if(d != 0)
				perLineExecute = true;
			break;
		}
		else
		{
			cout << "Unknown instruction" << endl;
			if(labelAdded)
				labelList.pop_back();
		}

		linecount++;
	}
	for(int i = branchInstructionsToLink.size() - 1; i >= 0; i--)
	{
		QINSTR* instr = branchInstructionsToLink[i];
		string s = branchinstructionsToLink_LabelNames[i];
		bool flag = false;
		for(int j = labelList.size() - 1; j >= 0; j--)
		{
			LABEL* l = labelList[j];
			if(s.compare(l->name) == 0)
			{
				flag = true;
				if(instr->instrcution == ::Instructions::JMP)
					instr->value = l->line;
				else
					instr->value = l->line - instr->line;
				break;
			}
		}
		if(!flag)
		{
			string s = string("Cannot link ").append(instructionEnumToString(instr->instrcution)).append(" to label, please check line ").append(to_string((long long)instr->line));
			cleanup();
			MessageBoxA(NULL, s.c_str(), "ERROR", MB_OK | MB_ICONSTOP);
			exit(1);
		}
	}
	branchInstructionsToLink.clear();
	branchinstructionsToLink_LabelNames.clear();
#pragma endregion
#pragma region Simulate ALU
	int lastLine = 0;
	unsigned int cycles = 0;
	for(int line = 0; line < linecount; line++)
	{
		if(line > instructionList.size())
		{
			string s = string("line '").append(to_string((long long)line)).append("' is out of range! Error occured at line: ").append(to_string((long long)lastLine));
			MessageBoxA(NULL, s.c_str(), "ERROR", MB_OK | MB_ICONSTOP);
		}
		QINSTR* instr = instructionList[line];

		lastLine = line;
		switch(instr->instrcution)
		{
		case ::Instructions::LDA_ST:
			currentValue = stack[instr->value];
			break;
		case ::Instructions::LDA_VA:
			currentValue = instr->value;
			break;
		case ::Instructions::STA:
			stack[instr->value] = currentValue;
			break;

		case ::Instructions::ADD_ST:
			currentValue += stack[instr->value];
			stateRegister.carry = false;
			if(currentValue > 127)
			{
				currentValue = -128;
				stateRegister.carry = true;
			}
			if(currentValue < -128)
			{
				currentValue = 127;
				stateRegister.carry = true;
			}
			stateRegister.negation = (currentValue < 0);
			stateRegister.zero = (currentValue == 0);
			break;
		case ::Instructions::ADD_VA:
			currentValue += instr->value;
			stateRegister.carry = false;
			if(currentValue > 127)
			{
				currentValue = -128;
				stateRegister.carry = true;
			}
			if(currentValue < -128)
			{
				currentValue = 127;
				stateRegister.carry = true;
			}
			stateRegister.negation = (currentValue < 0);
			stateRegister.zero = (currentValue == 0);
			break;
		case ::Instructions::SUB_ST:
			currentValue -= stack[instr->value];
			stateRegister.carry = false;
			if(currentValue > 127)
			{
				currentValue = -128;
				stateRegister.carry = true;
			}
			if(currentValue < -128)
			{
				currentValue = 127;
				stateRegister.carry = true;
			}
			stateRegister.negation = (currentValue < 0);
			stateRegister.zero = (currentValue == 0);
			break;
		case ::Instructions::SUB_VA:
			currentValue -= instr->value;
			stateRegister.carry = false;
			if(currentValue > 127)
			{
				currentValue = -128;
				stateRegister.carry = true;
			}
			if(currentValue < -128)
			{
				currentValue = 127;
				stateRegister.carry = true;
			}
			stateRegister.negation = (currentValue < 0);
			stateRegister.zero = (currentValue == 0);
			break;
			
		case ::Instructions::JMP:
			line = instr->value - 1;
			break;
		case ::Instructions::BRZ:
			if(stateRegister.zero)
				line += instr->value - 1;
			break;
		case ::Instructions::BRC:
			if(stateRegister.carry)
				line += instr->value - 1;
			break;
		case ::Instructions::BRN:
			if(stateRegister.negation)
				line += instr->value - 1;
			break;
		}
		if(perLineExecute)
		{
			cout << "STATE REPORT:" << endl <<
				"Register 10: " << stack[10] << endl <<
				"Register 11: " << stack[11] << endl <<
				"Register 12: " << stack[12] << endl <<
				"Register 13: " << stack[13] << endl <<
				"Register 14: " << stack[14] << endl <<
				"Register 15: " << stack[15] << endl <<
				"Register 16: " << stack[16] << endl <<
				"Register 17: " << stack[17] << endl <<
				"Register 18: " << stack[18] << endl <<
				"Register 19: " << stack[19] << endl <<
				"Register 20: " << stack[20] << endl <<
				"CARRY Flag: " << stateRegister.carry << endl <<
				"ZERO Flag: " << stateRegister.zero << endl <<
				"NEGATION Flag: " << stateRegister.negation << endl <<
				"Current Value: " << currentValue << endl << 
				"last line: " << lastLine << endl <<
				"next line: " << line + 1 << endl <<
				"next instruction: " << instructionEnumToString(instructionList.size() <= line + 1 ? -1 : instructionList[line + 1]->instrcution) << endl << 
				"cylces: " << cycles << endl;
			system("PAUSE");
		}
		cycles++;
	}
	cout << "Programm terminated after " << cycles << " cycles" << endl;
	system("PAUSE");
#pragma endregion
}