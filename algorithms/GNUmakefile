DOC = $(wildcard *.tex)
PDF = $(DOC:%.tex=%.pdf)

all : $(PDF)

%.pdf : %.tex
	pdftex $(<)

clean :
	rm -f $(PDF) *.log *.aux
