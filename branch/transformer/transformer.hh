#include <iostream>
#include <fstream>
#include <array>
#include <stdexcept>
#include <string>
#include <cmath>
#include <json.hpp> // Nlohmann-json dep


using json = nlohmann::json;

class TransformerBase {
    protected:
        int input_dim;             // Input Dimensionality (64 bit IP)
        int pos_encoding_dim;      // Dimension after positional encoding is appended (96-bit, ignore 32 MSB's)
        int num_mma_heads;         // Number of Masked Multi-headed Attention Heads
        int num_ma_heads;          // Number of Multi-headed attention heads
        int d_model;               // Embeding dimension 
        int d_ff;                  // Feed-Forward layer size
        float dropout_rate;        // Dropout rate
        int sequence_len;          // Number of previous instructions passed in as input 

    public:
        // Construct the transformer from a given input configuration file
        TransformerBase(const std::string &config_file) {
            json config = loadConfig(config_file);
            input_dim = config["input_dim"];
            pos_encoding_dim = config["pos_encoding_dim"];
            num_mma_heads = config["num_mma_heads"];
            num_ma_heads = config["num_ma_heads"];
            d_model = config["d_model"];
            d_ff = config["d_ff"];
            dropout_rate = config["dropout_rate"];
            sequence_len = config["sequence_len"];
        }

        virtual ~TransformerBase() = default;

        json loadConfig(const std::string &config_file) {
            std::ifstream file(config_file);
            if (!file.is_open()){
                throw std::runtime_error("Could not open config file.");
            }

            return json::parse(file);
        }

        // Virtual function implementations
        // Convert to vector of vectors, and later, of more vectors (d_model * sequence_len * batch)
        virtual std::array<std::array<float, 96>, sequence_len> positionalEncoding(const uint64_t input) = 0; 
        // At this point, it's still a binary vector (0, 1), we don't utilize the float until further steps. 
        virtual std::array<float, 128> MMALayer(std::array<float, 96> &input) = 0;
        virtual std::array<float, 128> MALayer(
            const std::array<float, 128> &query,
            const std::array<float, 128> &key,
            const std::array<float, 128> &value            
            ) = 0;
        virtual std::array<float, 256> feedForwardLayer(const std::array<float, 128> &input) = 0;
        virtual std::array<float, 128> layerNormalization(const std::array<float, 128> &input) = 0;

        virtual bool predict(uint64_t input) = 0; // Final output, branch taken, or not

};