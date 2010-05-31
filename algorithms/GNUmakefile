DOC = $(wildcard *.tex)
PDF = $(DOC:%.tex=%.pdf)

all : $(PDF)

%.pdf : %.tex
	pdftex $(<)
	rm $(<:%.tex=%.log)

clean :
	rm -f $(PDF) *.log *.aux
