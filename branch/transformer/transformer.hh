// transformer.h

#ifndef TRANSFORMER_H
#define TRANSFORMER_H

#include <functional>
#include <memory>
#include <vector>

#include <torch/torch.h>

// Function to create N clones of a module
template <typename ModuleType>
std::vector<ModuleType> clones(ModuleType module, int64_t N) {
  std::vector<ModuleType> modules;
  for (int64_t i = 0; i < N; ++i) {
    auto cloned_impl = std::dynamic_pointer_cast<typename ModuleType::Impl>(module->clone());
    modules.push_back(ModuleType(cloned_impl));
  }
  return modules;
}

// SublayerConnection module with residual connection and dropout
class SublayerConnectionImpl : public torch::nn::Cloneable<SublayerConnectionImpl> {
public:
  SublayerConnectionImpl() = default;
  SublayerConnectionImpl(int64_t size, double p_dropout)
    : size(size), dropout_p(p_dropout) {
    reset();
  }

  void reset() override {
    norm = register_module("norm", torch::nn::LayerNorm(torch::nn::LayerNormOptions({ size })));
    dropout = register_module("dropout", torch::nn::Dropout(dropout_p));
  }

  void pretty_print(std::ostream& stream) const override {
    stream << "SublayerConnection(size=" << size << ", dropout_p=" << dropout_p << ")";
  }

  torch::Tensor forward(torch::Tensor x, const std::function<torch::Tensor(torch::Tensor)>& sublayer) {
    return x + dropout(sublayer(norm->forward(x)));
  }

private:
  int64_t size;
  double dropout_p;
  torch::nn::LayerNorm norm{ nullptr };
  torch::nn::Dropout dropout{ nullptr };
};
TORCH_MODULE(SublayerConnection);

// EncoderLayer
class EncoderLayerImpl : public torch::nn::Cloneable<EncoderLayerImpl> {
public:
  EncoderLayerImpl(int64_t size, torch::nn::MultiheadAttention self_attn,
    torch::nn::Sequential feed_forward, double dropout_p)
    : size(size), dropout_p(dropout_p), self_attn(self_attn), feed_forward(feed_forward) {
  }

  void reset() override {
    sublayer = clones<SublayerConnection>(SublayerConnection(size, dropout_p), 2);
    for (size_t i = 0; i < sublayer.size(); ++i) {
      register_module("sublayer_" + std::to_string(i), sublayer[i]);
    }
    register_module("self_attn", self_attn);
    register_module("feed_forward", feed_forward);
  }

  torch::Tensor forward(torch::Tensor x, torch::Tensor mask) {
    x = sublayer[0](x, [&](torch::Tensor x) {
      return std::get<0>(self_attn->forward(x, x, x, mask));
      });
    x = sublayer[1](x, [&](torch::Tensor x) {
      return feed_forward->forward(x);
      });
    return x;
  }

  void pretty_print(std::ostream& stream) const override {
    stream << "EncoderLayer(size=" << size << ", dropout_p=" << dropout_p << ")";
  }

  int64_t get_size() const { return size; }

private:
  int64_t size;
  double dropout_p;
  torch::nn::MultiheadAttention self_attn{ nullptr };
  torch::nn::Sequential feed_forward{ nullptr };
  std::vector<SublayerConnection> sublayer;
};
TORCH_MODULE(EncoderLayer);


// Encoder
class EncoderImpl : public torch::nn::Cloneable<EncoderImpl> {
public:
  EncoderImpl(EncoderLayer layer, int64_t N)
    : layer(layer), N(N), size(layer->get_size()) {
  }

  void reset() override {
    layers = clones<EncoderLayer>(layer, N);
    for (size_t i = 0; i < layers.size(); ++i) {
      register_module("layer_" + std::to_string(i), layers[i]);
    }
    norm = register_module("norm", torch::nn::LayerNorm(torch::nn::LayerNormOptions({ size })));
  }

  torch::Tensor forward(torch::Tensor x, torch::Tensor mask) {
    for (auto& layer : layers) {
      x = layer->forward(x, mask);
    }
    return norm->forward(x);
  }

  void pretty_print(std::ostream& stream) const override {
    stream << "Encoder(size=" << size << ", N=" << N << ")";
  }

  int64_t get_size() const { return size; }

private:
  EncoderLayer layer;
  int64_t N;
  int64_t size;
  std::vector<EncoderLayer> layers;
  torch::nn::LayerNorm norm{ nullptr };
};
TORCH_MODULE(Encoder);



// PositionalEncoding
class PositionalEncodingImpl : public torch::nn::Module
{
public:
  PositionalEncodingImpl(int64_t d_model, double dropout, int64_t max_len = 5000) : dropout(torch::nn::Dropout(dropout))
  {
    // Compute the positional encodings once in log space.
    torch::Tensor pe = torch::zeros({ max_len, d_model });
    torch::Tensor position = torch::arange(0, max_len).unsqueeze(1);
    torch::Tensor div_term = torch::exp(torch::arange(0, d_model, 2) * -(std::log(10000.0) / d_model));
    pe.slice(1, 0, d_model, 2) = torch::sin(position * div_term);
    pe.slice(1, 1, d_model, 2) = torch::cos(position * div_term);
    pe = pe.unsqueeze(0);
    register_buffer("pe", pe);
  }

  torch::Tensor forward(torch::Tensor x)
  {
    x = x + pe.slice(1, 0, x.size(1));
    return dropout->forward(x);
  }

private:
  torch::nn::Dropout dropout;
  torch::Tensor pe;
};
TORCH_MODULE(PositionalEncoding);

// Embedding with scaling
class EmbeddingsImpl : public torch::nn::Module
{
public:
  EmbeddingsImpl(int64_t d_model, int64_t vocab) : lut(torch::nn::Embedding(vocab, d_model)), d_model(d_model) { register_module("lut", lut); }

  torch::Tensor forward(torch::Tensor x) { return lut->forward(x) * std::sqrt(d_model); }

private:
  torch::nn::Embedding lut;
  int64_t d_model;
};
TORCH_MODULE(Embeddings);

// Full Transformer Model
class TransformerBranchPredictorImpl : public torch::nn::Cloneable<TransformerBranchPredictorImpl> {
public:
  TransformerBranchPredictorImpl(int64_t src_vocab, int64_t tgt_vocab, int64_t d_model, int64_t N,
    int64_t h, int64_t d_ff, double dropout_p = 0.1)
    : src_vocab(src_vocab),
    tgt_vocab(tgt_vocab),
    d_model(d_model),
    N(N),
    h(h),
    d_ff(d_ff),
    dropout_p(dropout_p) {
  }

  void reset() override {
    src_embed = register_module("src_embed", torch::nn::Embedding(src_vocab, d_model));
    pos_encoder = register_module("pos_encoder", PositionalEncoding(d_model, dropout_p));

    encoder_layer = EncoderLayer(
      d_model,
      torch::nn::MultiheadAttention(d_model, h),
      torch::nn::Sequential(
        torch::nn::Linear(d_model, d_ff),
        torch::nn::Functional(torch::relu),
        torch::nn::Linear(d_ff, d_model)),
      dropout_p);

    encoder = register_module("encoder", Encoder(encoder_layer, N));
    fc_out = register_module("fc_out", torch::nn::Linear(d_model, tgt_vocab));
  }

  torch::Tensor forward(torch::Tensor src, torch::Tensor src_mask) {
    torch::Tensor x = src_embed(src);
    x = pos_encoder(x);
    x = encoder->forward(x, src_mask);
    x = x.mean(1); // Global average pooling
    x = fc_out->forward(x);
    return torch::sigmoid(x);
  }

  void pretty_print(std::ostream& stream) const override {
    stream << "TransformerBranchPredictor";
  }

private:
  int64_t src_vocab, tgt_vocab, d_model, N, h, d_ff;
  double dropout_p;
  torch::nn::Embedding src_embed{ nullptr };
  PositionalEncoding pos_encoder{ nullptr };
  EncoderLayer encoder_layer{ nullptr };
  Encoder encoder{ nullptr };
  torch::nn::Linear fc_out{ nullptr };
};
TORCH_MODULE(TransformerBranchPredictor);



#endif // TRANSFORMER_H
