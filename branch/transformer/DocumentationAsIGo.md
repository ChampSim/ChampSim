## Positional Encoder

Positional encoders encode the relative position of an incoming token __*ip in our case*__. This is needed to prevent the transformer from attenuating to irrelvant or out-of-sequence tokens, ensuring a sequential nature to it's thinking. "" *I'm here now, I came from here, therefore I will go there* "" 


Normally, positional encoders have a maximum, you can see this in max context window size. However, since we're implimenting this into a cpu we need a system which can "wrap" but still maintain correct psoitional awareness. 


#### Fixed-range positional encoding with modulo arithmetic:

By introducing a mechansim to disambiguate overlapping positions we can still work with a fixed range (e.g: 0 to 10,000 - 1) with a modulo operation.

**Approach:**
- **Modulo encoding:** Use the positional encoding for (pos mod 10,000)
- **Disambiguation:** Introduce a small secondary feature, (counter or embeddeding) to indicate wrap-number

**Formula:**

$$PE(pos)=Sinusoidal((pos mod 10,000)) + WrapEmbedding(⌊pos / 10,000⌋)$$

- **Sinusoidal((pos mod 10,000))**: Standard sin encoding with fixed range
- **WrapEmbedding(|pos / 10,000|)**: Small learnable vector or constant offset for each wrap. 

**Advantages:** Mod is easy on hardware & wrap counter adds minimal complexity.


#### Relative 
