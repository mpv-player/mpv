#! /bin/make -f
#
# Makefile to convert the example.sgml

# old multiple commands way
# COMMAND=sgml2

# new single command approach
COMMAND=linuxdoc --backend=

SOURCE=documentation.sgml

all: txt html tex dvi ps pdf info lyx rtf
	echo "all output formats are created!"


txt:
	$(COMMAND)$@ $(SOURCE)

html:
	$(COMMAND)$@ $(SOURCE)

tex:
	$(COMMAND)latex -o $@ $(SOURCE)

dvi:
	$(COMMAND)latex -o $@ $(SOURCE)

ps:
	$(COMMAND)latex -o $@ $(SOURCE)

pdf:
	$(COMMAND)latex -o $@ $(SOURCE)

info:
	$(COMMAND)$@ $(SOURCE)

lyx:
	$(COMMAND)$@ $(SOURCE)

rtf:
	$(COMMAND)$@ $(SOURCE)

clean:
	rm -f *.txt *.html *.tex *.dvi *.ps *.pdf *.info *.lyx *.rtf

