.PHONY: docs clean
docs: 
	doxygen ./docs/Doxyfile
clean:
	rm -rfv ./docs/html