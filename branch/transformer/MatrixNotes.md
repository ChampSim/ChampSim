# Various Matrix Shapes
## Matrix Dimensions in Attention
To clarify the dimensions of various matrices in self-attention:
`
Component	                | Shape	                  | Explanation
----------------------------|-------------------------|---------------------
Input (sequence_history)	| [sequence_len, d_model] |	Input embeddings for each token in the sequence.
Weight Matrices 𝑊_𝑄, 𝑊_𝐾, 𝑊_𝑉 | [d_model, d_model]  | Project input embeddings into queries, keys, and values.
Queries 𝑄 | [sequence_len, d_model]  |	Transformed input into queries.
Keys 𝐾    | [sequence_len, d_model]  |	Transformed input into keys.
Values 𝑉  | [sequence_len, d_model]  |	Transformed input into values.
Attention Scores 𝑄 * 𝐾^𝑇 | [sequence_len, sequence_len]	| Pairwise similarity between all tokens (computed for softmax normalization).
Softmax Output	| [sequence_len, sequence_len] |	Normalized attention scores (one row for each token's distribution over the sequence).
Weighted Sum of Values |	[sequence_len, d_model] |	Final attention output, combining values using attention scores for each token.
