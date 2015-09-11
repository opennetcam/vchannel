TARGETDEB=opennetcam_1.0*.deb

all: clean prebuild build cleanup
	@echo done

prebuild:
	@cp -uv ../bin/vchannel .
	strip vchannel     
	@cp -uv ../man/vchannel.1 .   
	@gzip -9 vchannel.1

build:
	equivs-build opennetcam-debian

cleanup:
	@rm -f vchannel      
	@rm -f vchannel.1.gz      

clean:
	@rm -f ${TARGETDEB}
	@rm -f *~
	

