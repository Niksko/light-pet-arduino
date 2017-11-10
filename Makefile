# Define the path to the nanopb generator for the protoc compiler
NANOPB_GENERATOR_PATH = nanopb/generator/protoc-gen-nanopb

CLIENT_SRC_DIR = src
TOOLS_DIR = tools

all: client

client: client_config client_protobuf

client_config: configuration.json
	python $(TOOLS_DIR)/write_config_header.py configuration.json
	mv configuration.h $(CLIENT_SRC_DIR)

client_protobuf: $(CLIENT_SRC_DIR)/sensorData.proto
	protoc -I=$(CLIENT_SRC_DIR) --plugin=protoc-gen-nanopb=$(NANOPB_GENERATOR_PATH) --nanopb_out=$(CLIENT_SRC_DIR) $(CLIENT_SRC_DIR)/sensorData.proto

.PHONY: clean

clean:
	-rm $(CLIENT_SRC_DIR)/configuration.h
	-rm $(CLIENT_SRC_DIR)/sensorData.pb.*
