MAKE=make
.DEFAULT_GOAL := default

JSONCPP_VERSION=1.9.5
JSONCPP_DIR=jsoncpp-$(JSONCPP_VERSION)
JSONCPP_LIB=$(JSONCPP_DIR)/build/lib/libjsoncpp.a

$(JSONCPP_DIR):
	@echo
	@echo "=== Downloading jsoncpp $(JSONCPP_VERSION) ==="
	@echo
	curl "https://codeload.github.com/open-source-parsers/jsoncpp/tar.gz/refs/tags/${JSONCPP_VERSION}" | tar xz

$(JSONCPP_LIB): 
	mkdir $(JSONCPP_DIR)/build
	cd $(JSONCPP_DIR)/build; cmake3 ..
	$(MAKE) -C $(JSONCPP_DIR)/build 

$(JSONCPP_DIR)/clean:
	$(MAKE) -C $(JSONCPP_DIR)/build clean
	rm -rf $(JSONCPP_DIR)/build

$(JSONCPP_DIR)/install:
	@echo 'install jsoncpp'
	cp $(JSONCPP_LIB) ../lib
	rm -rf ../include/json
	cp -rf $(JSONCPP_DIR)/include/json ../include

HTTPPARSER_VERSION=2.9.4
HTTPPARSER_DIR=http-parser-$(HTTPPARSER_VERSION)
HTTPPARSER_LIB=$(HTTPPARSER_DIR)/libhttp_parser.a

$(HTTPPARSER_DIR):
	@echo
	@echo "=== Downloading http-parser $(HTTPPARSER_VERSION) ==="
	@echo
	curl "https://codeload.github.com/nodejs/http-parser/tar.gz/refs/tags/v$(HTTPPARSER_VERSION)" | tar xz

$(HTTPPARSER_LIB): 
	$(MAKE) -C $(HTTPPARSER_DIR) package

$(HTTPPARSER_DIR)/clean:
	$(MAKE) -C $(HTTPPARSER_DIR) clean

$(HTTPPARSER_DIR)/install:
	@echo 'install http-parser'
	cp $(HTTPPARSER_LIB) ../lib 
	rm -rf ../include/http-parser;mkdir ../include/http-parser
	cp -rf $(HTTPPARSER_DIR)/*.h ../include/http-parser

fetch: $(JSONCPP_DIR) $(HTTPPARSER_DIR)

default: $(JSONCPP_LIB) $(HTTPPARSER_LIB)

clean: $(JSONCPP_DIR)/clean

.PYHONY: default fetch clean

install: $(JSONCPP_DIR)/install $(HTTPPARSER_DIR)/install
