#!/usr/bin/python

allops = ["A", "B", "C", "X", "Y", "Z", "I", "J", "[A]", "[B]", "[C]", "[X]", "[Y]", "[Z]", "[I]", "[J]", "[NW+A]", "[NW+B]", "[NW+C]", "[NW+X]", "[NW+Y]", "[NW+Z]", "[NW+I]", "[NW+J]", "[SP++]", "[SP]", "[--SP]", "SP", "PC", "O", "[NW]", "NW", "0x00", "0x1F", "0xffff", "0", "65535"]

allins = ["SET", "ADD", "SUB", "MUL", "DIV", "MOD", "SHL", "SHR", "AND", "BOR", "XOR", "IFE", "IFN", "IFG", "IFB"]

allins_ext = ["JSR", "SYS"]

for ins in allins:
	for op1 in allops:
		for op2 in allops:
			op1 = op1.replace("NW", "0xaaaa");
			op2 = op2.replace("NW", "0xaaaa");
			print "%s %s, %s" % (ins, op1, op2)

for ins in allins_ext:
	for op in allops:
		op = op.replace("NW", "0xaaaa");
		print "%s %s" % (ins, op)
