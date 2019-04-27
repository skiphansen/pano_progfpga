all: patched patch_progfpga

patch_progfpga:

patched:
	mkdir patched
	cp -a series1 patched
	cp -a series2 patched

clean:
	-rm *.o
	-rm patch_progfpga
	-rm -rf patched

