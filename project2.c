#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static const int maxMemCount = 32768;
int memCount = 0;

int commentCount = 0;
int lineCount = 0;

char errorLine[1024];

static const char* opTable[] =
{
	"ADD",    "ADDF",   "ADDR",
	"AND",    "CLEAR",  "COMP",
	"COMPF",  "COMPR",  "DIV",
	"DIVF",   "DIVR",   "FIX",
	"FLOAT",  "HIO",    "J",
	"JEQ",    "JGT",    "JLT",
	"JSUB",   "LDA",    "LDB",
	"LDCH",   "LDF",    "LDL",
	"LDS",    "LDT",    "LDX",
	"LPS",    "MUL",    "MULF",
	"MULR",   "NORM",   "OR",
	"RD",     "RMO",    "RSUB",
	"SHIFTL", "SHIFTR", "SIO",
	"SSK",    "STA",    "STB",
	"STCH",   "STF",    "STI",
	"STL",    "STS",    "STSW",
	"STT",    "STX",    "SUB",
	"SUBF",   "SUBR",   "SVC",
	"TD",     "TIO",    "TIX",
	"TIXR",   "WD"
};

static const char* opVals[] =
{
	"18", "58", "90",
	"40", "B4", "28",
	"88", "A0", "24",
	"64", "9C", "C4",
	"C0", "F4", "3C",
	"30", "34", "38",
	"48", "00", "68",
	"50", "70", "08",
	"6C", "74", "04",
	"D0", "20", "60",
	"98", "C8", "44",
	"D8", "AC", "4C",
	"A4", "A8", "F0",
	"EC", "0C", "78",
	"54", "80", "D4",
	"14", "7C", "E8",
	"84", "10", "1C",
	"5C", "94", "B0",
	"E0", "F8", "2C",
	"B8", "DC"
};

void printErrorMsg(char* msg)
{
	printf("ASSEMBLY ERROR: %s\t---->Line %d : %s\n", errorLine, lineCount + commentCount, msg);
}

int hexTOdec(char* hex)
{
	int length = strlen(hex);

	int decVal = 0;
	int base = 1;

	for (int i = length - 1; i >= 0; i--)
	{
		if (hex[i] >= '0' && hex[i] <= '9')
		{
			decVal += (hex[i] - 48) * base;
			base *= 16;
		}
		else if (hex[i] >= 'A' && hex[i] <= 'F')
		{
			decVal += (hex[i] - 55) * base;
			base *= 16;
		}
	}

	return decVal;
}

void decTOhex(int dec, char* hex)
{
	int i = strlen(hex) - 1;
	while (dec != 0)
	{
		int temp = dec % 16;
		if (temp < 10)
		{
			hex[i] = temp + 48;
			i--;
		}
		else
		{
			hex[i] = temp + 55;
			i--;
		}
		dec /= 16;
	}
}

struct Node
{
	int memAddr;
	char* symbol;
};

struct Node* hashTable[1009];

int hashFunc(char* symbol)
{
	unsigned long hash = 0;
	int c;

	while (c = *symbol++)
		hash = c + (hash << 6) + (hash << 16) - hash;

	return hash % 1009;
}

struct Node* hashSearch(char* symbol)
{
	int hashIndex = hashFunc(symbol);

	while (hashTable[hashIndex] != NULL)
	{
		if (strcmp(hashTable[hashIndex]->symbol, symbol) == 0)
			return hashTable[hashIndex];

		++hashIndex;
		hashIndex %= 1009;
	}

	return NULL;
}

void hashInsert(char* symbol)
{
	struct Node* node = (struct Node*)malloc(sizeof(struct Node));
	node->symbol = (char*)malloc(7 * sizeof(char));
	strcpy(node->symbol, symbol);
	node->memAddr = memCount;

	int hashIndex = hashFunc(symbol);

	while (hashTable[hashIndex] != NULL)
	{
		++hashIndex;
		hashIndex %= 1009;
	}

	hashTable[hashIndex] = node;
}

bool checkOpCode(char* opCode)
{
	for (int i = 0; i < 59; i++)
	{
		if (strcmp(opCode, opTable[i]) == 0)
			return true;
	}

	return false;
}

void getOpVal(char* opCode, char* opVal)
{
	for (int i = 0; i < 59; i++)
	{
		if (strcmp(opCode, opTable[i]) == 0)
			strcpy(opVal, opVals[i]);
	}
}

bool checkSymbol(char* symbol)
{
	if (strlen(symbol) > 6)
	{
		printErrorMsg("SYMBOL LENGTH LONGER THAN 6");
		return false;
	}
	if ((strcmp(symbol, "START") == 0) ||
		(strcmp(symbol, "END") == 0) ||
		(strcmp(symbol, "BYTE") == 0) ||
		(strcmp(symbol, "WORD") == 0) ||
		(strcmp(symbol, "RESB") == 0) ||
		(strcmp(symbol, "RESW") == 0) ||
		(strcmp(symbol, "RESR") == 0))
	{
		printErrorMsg("SYMBOL CANNOT BE A DIRECTIVE");
		return false;
	}
	if (strstr(symbol, "$") ||
		strstr(symbol, "!") ||
		strstr(symbol, "=") ||
		strstr(symbol, "+") ||
		strstr(symbol, "-") ||
		strstr(symbol, "(") ||
		strstr(symbol, ")") ||
		strstr(symbol, "@"))
	{
		printErrorMsg("SYMBOL CONTAINS INVALID CHARACTER");
		return false;
	}
	return true;
}

bool addToMem(char* opCode, char* value)
{
	if (checkOpCode(opCode))
		memCount += 3;
	else if (strcmp(opCode, "WORD") == 0)
	{
		if (atoi(value) > 8388608)
		{
			printErrorMsg("CONSTANT EXCEEDS 'WORD' SIZE");
			return false;
		}
		memCount += 3;
	}
	else if (strcmp(opCode, "RESW") == 0)
		memCount += (3 * atoi(value));
	else if (strcmp(opCode, "RESB") == 0)
		memCount += atoi(value);
	else if (strcmp(opCode, "BYTE") == 0)
	{
		if (value[0] == 'C')
			memCount += (strlen(value) - 3);
		else if (value[0] == 'X')
		{
			for (int i = 2; i < strlen(value) - 1; i++)
			{
				if ((value[i] >= '0' && value[i] <= '9') || (value[i] >= 'A' && value[i] <= 'F'))
				{
					//do nothing
				}
				else
				{
					printErrorMsg("INVALID BYTE EXISTS");
					return false;
				}
			}
			memCount += (strlen(value) - 3) / 2;
		}
		else
		{
			printErrorMsg("'BYTE' SHOULD BE FOLLOWED BY 'C' OR 'X'");
			return false;
		}
	}
	else if (strcmp(opCode, "END") == 0)
		return true;
	else if (strcmp(opCode, "START") == 0)
		return true;
	else
	{
		printErrorMsg("INVALID OPCODE");
		return false;
	}

	if (memCount > maxMemCount)
	{
		printErrorMsg("MAXIMUM MEMORY COUNT HAS BEEN EXCEDED");
		return false;
	}

	return true;
}

int main(int argc, char* argv[])
{
	if (argc != 2)
	{
		printf("USAGE: %s <filename>\n", argv[0]);
		return 1;
	}

	FILE* inputFile = fopen(argv[1], "r");

	if (!inputFile)
	{
		printf("ERROR: %s could not be opened for reading\n", argv[1]);
		return 1;
	}

	char* startMemAddr = (char*)malloc(5 * sizeof(char));
	char* finalMemAddr = (char*)malloc(5 * sizeof(char));

	char line[1024];
	char lineCopy1[1024];
	char lineCopy2[1024];

	char* opCode = (char*)malloc(7 * sizeof(char));
	char* symbol = (char*)malloc(7 * sizeof(char));
	char hexMem[] = "0000";

	bool endFound = false;

	//Pass1
	while (fgets(line, 1024, inputFile))
	{
		strcpy(errorLine, line);
		strcpy(lineCopy1, line);
		//strcpy(lineCopy2, line);
		int length = strlen(line);

		if (line[0] == '\r' || line[0] == '\n')
		{
			//do nothing
		}
		else if (length > 0)
		{
			if (line[0] == '#')
				commentCount++;
			else
			{
				lineCount++;

				if (lineCount == 1)
				{
					char* token = strtok(line, " \t\n\r");
					strcpy(symbol, token);
					token = strtok(NULL, " \t\n\r");
					strcpy(opCode, token);
					token = strtok(NULL, " \t\n\r");
					strcpy(hexMem, token);
					strcpy(startMemAddr, token);

					if (strcmp(opCode, "START") != 0)
					{
						printErrorMsg("START DIRECTIVE NOT FOUND");
						return 1;
					}
					else
					{
						memCount = hexTOdec(hexMem);
						hashInsert(symbol);

						//printf("%s\t%s\n", symbol, hexMem);
					}
				}
				else
				{
					if (strstr(line, "END"))
					{
						char* token = strtok(lineCopy1, " \t\n\r");
						while (token)
						{
							if (strcmp(token, "END") == 0)
							{
								if (endFound)
								{
									printErrorMsg("DUPLICATE 'END' DIRECTIVE");
									return 1;
								}
								endFound = true;
								decTOhex(memCount, hexMem);
								strcpy(finalMemAddr, hexMem);
							}
							token = strtok(NULL, " \t\n\r");
						}
					}

					if (line[0] >= 'A' && line[0] <= 'Z')
					{
						char* token = strtok(line, " \t\n\r");
						strcpy(symbol, token);
						token = strtok(NULL, " \t\n\r");
						strcpy(opCode, token);
						token = strtok(NULL, "\t\n\r");

						if (!checkSymbol(symbol))
							return 1;
						if (hashSearch(symbol))
						{
							printErrorMsg("DUPLICATE SYMBOL");
							return 1;
						}
						if (strcmp(opCode, "START") == 0)
						{
							printErrorMsg("DUPLICATE 'START' DIRECTIVE");
							return 1;
						}

						decTOhex(memCount, hexMem);
						hashInsert(symbol);

						//printf("%s\t%s\n", symbol, hexMem);

						if (!addToMem(opCode, token))
							return 1;
					}
					else
					{
						char* token = strtok(line, " \t\n\r");
						strcpy(opCode, token);
						token = strtok(NULL, "\t\n\r");

						if (!addToMem(opCode, token))
							return 1;
					}
				}
			}
		}
	}
	if (!endFound)
	{
		printErrorMsg("'END' DIRECTIVE NOT FOUND");
		return 1;
	}

	rewind(inputFile);
	memCount = hexTOdec(startMemAddr);
	commentCount = 0;
	lineCount = 0;
	endFound = false;

	//char* outputFile = strcat(strtok(argv[1], "."), ".obj");
	char* outputFile = strcat(argv[1], ".obj");
	FILE* objectFile = fopen(outputFile, "w");

	char* textRecord = (char*)malloc(70 * sizeof(char));
	char* objectCodeString = (char*)malloc(61 * sizeof(char));
	char* objectCodeString2 = (char*)malloc(61 * sizeof(char));

	char* opVal = (char*)malloc(3 * sizeof(char));

	bool foundFirstExecutable = false;
	struct Node* firstExecutable = (struct Node*)malloc(sizeof(struct Node));

	//Pass2
	while (fgets(line, 1024, inputFile))
	{
		strcpy(hexMem, "0000");

		strcpy(errorLine, line);
		strcpy(lineCopy1, line);
		strcpy(lineCopy2, line);
		int length = strlen(line);

		if (line[0] == '\r' || line[0] == '\n')
		{
			//do nothing
		}
		else if (length > 0)
		{
			char tokenCopy[1024];

			if (line[0] == '#')
				commentCount++;
			else
			{
				lineCount++;

				if (lineCount == 1)
				{
					int programLength = hexTOdec(finalMemAddr) - hexTOdec(startMemAddr);
					char* token = strtok(line, " \t\n\r");

					//printf("H%-6s%06X%06X\n", token, hexTOdec(startMemAddr), programLength);
					fprintf(objectFile, "H%-6s%06X%06X\n", token, hexTOdec(startMemAddr), programLength);
				}
				else
				{
					if (strstr(line, "END"))
					{
						char* token = strtok(lineCopy1, " \t\n\r");
						while (token)
						{
							if (strcmp(token, "END") == 0)
							{
								endFound = true;
								break;
							}
							token = strtok(NULL, " \t\n\r");
						}
						if (endFound)
						{
							if (line[0] >= 'A' && line[0] <= 'Z')
							{
								char* token = strtok(lineCopy2, " \t\n\r");
								token = strtok(NULL, " \t\n\r");
								token = strtok(NULL, " \t\n\r");

								if (!hashSearch(token))
								{
									printErrorMsg("UNDEFINED SYMBOL");
									remove(outputFile);
									return 1;
								}
								if (strcmp(token, firstExecutable->symbol) != 0)
								{
									printErrorMsg("NOT REFERENCING FIRST EXECUTABLE INSTRUCTION");
									remove(outputFile);
									return 1;
								}
							}
							else
							{
								char* token = strtok(lineCopy2, " \t\n\r");
								token = strtok(NULL, " \t\n\r");

								if (!hashSearch(token))
								{
									printErrorMsg("UNDEFINED SYMBOL");
									remove(outputFile);
									return 1;
								}
								if (strcmp(token, firstExecutable->symbol) != 0)
								{
									printErrorMsg("NOT REFERENCING FIRST EXECUTABLE INSTRUCTION");
									remove(outputFile);
									return 1;
								}
							}
						}
					}

					if (line[0] >= 'A' && line[0] <= 'Z')
					{
						memset(objectCodeString, 0, strlen(objectCodeString));
						memset(objectCodeString2, 0, strlen(objectCodeString2));

						char* token = strtok(line, " \t\n\r");
						strcpy(symbol, token);
						token = strtok(NULL, " \t\n\r");
						strcpy(opCode, token);
						token = strtok(NULL, "\t\n\r");
						if (token != NULL)
							strcpy(tokenCopy, token);

						if (checkOpCode(opCode))
						{
							if (!foundFirstExecutable)
							{
								firstExecutable = hashSearch(symbol);
								foundFirstExecutable = true;
							}

							getOpVal(opCode, opVal);
							strcat(objectCodeString, opVal);

							if (token != NULL)
							{
								char* token2 = strtok(token, ", ");

								if (!hashSearch(token2))
								{
									printErrorMsg("UNDEFINED SYMBOL");
									remove(outputFile);
									return 1;
								}

								struct Node* temp = hashSearch(token2);

								if (strstr(tokenCopy, ",X"))
								{
									decTOhex(temp->memAddr + 32768, hexMem);
									strcat(objectCodeString, hexMem);
								}
								else
								{
									decTOhex(temp->memAddr, hexMem);
									strcat(objectCodeString, hexMem);
								}
							}
						}
						else if (strcmp(opCode, "BYTE") == 0)
						{
							char* token2 = strtok(token, "'");
							char tempType = token2[0];
							token2 = strtok(NULL, "'");
							char tempHex[] = "00";

							if (strlen(token2) > 30)
							{
								if (tempType == 'C')
								{
									for (int i = 0; i < 30; i++)
									{
										decTOhex(token2[i], tempHex);
										strcat(objectCodeString, tempHex);
									}
									for (int i = 30; i < strlen(token2); i++)
									{
										decTOhex(token2[i], tempHex);
										strcat(objectCodeString2, tempHex);
									}
								}
								else
								{
									for (int i = 0; i < 30; i++)
										objectCodeString[strlen(objectCodeString)] = token2[i];
									for (int i = 30; i < strlen(token2); i++)
										objectCodeString2[strlen(objectCodeString2)] = token2[i];
								}
							}
							else
							{
								if (tempType == 'C')
								{
									char tempHex[] = "00";
									for (int i = 0; i < strlen(token2); i++)
									{
										decTOhex(token2[i], tempHex);
										strcat(objectCodeString, tempHex);
									}
								}
								else
								{
									strcat(objectCodeString, token2);
								}
							}
						}
						else if (strcmp(opCode, "WORD") == 0)
						{
							char tempHex[] = "000000";
							decTOhex(atoi(token), tempHex);
							strcat(objectCodeString, tempHex);
						}

						//printf("-%s\n", objectCodeString);
					}
					else
					{
						memset(objectCodeString, 0, strlen(objectCodeString));

						char* token = strtok(line, " \t\n\r");
						strcpy(opCode, token);
						token = strtok(NULL, " \t\n\r");
						if (token != NULL)
							strcpy(tokenCopy, token);

						getOpVal(opCode, opVal);
						strcat(objectCodeString, opVal);

						if (token != NULL)
						{
							char* token2 = strtok(token, ", ");

							//for (int i = 0; i < strlen(token2); i++)
								//printf("%c -> %d\n", token2[i], token2[i]);

							if (!hashSearch(token2))
							{
								printErrorMsg("UNDEFINED SYMBOL");
								remove(outputFile);
								return 1;
							}

							struct Node* temp = hashSearch(token2);

							if (strstr(tokenCopy, ",X"))
							{
								decTOhex(temp->memAddr + 32768, hexMem);
								strcat(objectCodeString, hexMem);
							}
							else
							{
								decTOhex(temp->memAddr, hexMem);
								strcat(objectCodeString, hexMem);
							}
						}
						else
							strcat(objectCodeString, "0000");

						//printf("-%s\n", objectCodeString);
					}
				}

				char byteCount[] = "00";
				decTOhex((strlen(textRecord) - 9) / 2, byteCount);
				textRecord[7] = byteCount[0];
				textRecord[8] = byteCount[1];

				if ((strcmp(opCode, "RESW") == 0 || strcmp(opCode, "RESB") == 0))
				{
					//printf("%s\n", textRecord);
					if (textRecord[9] != '\0')
						fprintf(objectFile, "%s\n", textRecord);
					memset(textRecord, 0, strlen(textRecord));
				}
				if (strlen(textRecord) + strlen(objectCodeString) > 69)
				{
					//printf("%s\n", textRecord);
					fprintf(objectFile, "%s\n", textRecord);
					memset(textRecord, 0, strlen(textRecord));
				}
				if (textRecord[0] == '\0' && objectCodeString[0] != '\0')
				{
					strcat(textRecord, "T00");
					//printf("%d - %X\n", memCount, memCount);
					decTOhex(memCount, hexMem);
					strcat(textRecord, hexMem);
					strcat(textRecord, "00");
				}

				if (objectCodeString2[0] != '\0')
				{
					strcat(textRecord, objectCodeString);
					textRecord[7] = '1';
					textRecord[8] = 'E';
					//printf("%s\n", textRecord);
					fprintf(objectFile, "%s\n", textRecord);
					memCount += 30;

					memset(textRecord, 0, strlen(textRecord));
					strcat(textRecord, "T00");
					decTOhex(memCount, hexMem);
					strcat(textRecord, hexMem);
					decTOhex(strlen(objectCodeString2) / 2, byteCount);
					strcat(textRecord, byteCount);
					strcat(textRecord, objectCodeString2);
					//printf("%s\n", textRecord);
					fprintf(objectFile, "%s\n", textRecord);
					memCount += strlen(objectCodeString2) / 2;
					memset(textRecord, 0, strlen(textRecord));
				}
				else
				{
					strcat(textRecord, objectCodeString);
					addToMem(opCode, tokenCopy);
				}

				if (endFound)
				{
					//printf("%s\n", textRecord);
					fprintf(objectFile, "%s\n", textRecord);
					//printf("E%06X\n", firstExecutable->memAddr);
					fprintf(objectFile, "E%06X\n", firstExecutable->memAddr);
					break;
				}
			}
		}
	}

	return 0;
}