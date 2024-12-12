# Largely taken from:
# https://towardsdatascience.com/how-to-estimate-the-number-of-parameters-in-transformer-models-ca0f57d8dff0


def mma_count(d_m: int, num_heads: int) -> int:
    # Q, K, V matrices per head + Output projection matrix
    return num_heads * ((d_m * d_m + d_m) + d_m * d_m)

def ff_count(d_m: int, d_f: int) -> int:
    return 2*d_m*d_f + d_m + d_f
def norm_count(d_m: int) -> int:
    return 2*d_m
def encoder_count(d_m: int, d_ff: int) -> int:
    return 4*d_m**2 + 2*d_m*d_ff + 9*d_m + d_ff
def decoder_count(d_m: int, d_ff: int) -> int:
    return 8*d_m**2 + 2*d_m*d_ff + 7*d_m + d_ff
def kb_memory_usage(params: int, bits: int, seq_len: int = 1) -> int:
    # This can be fixed
    return seq_len * (params * bits) / (8*1024)
def get_outputs(d: dict[str, int]) -> dict[str, int]:
    d_m: int = d["d_in"] + d["d_pos"]
    d_ff: int = d["mul_ff"] * d_m
    out = {
        "d_model": d_m,
        "d_ff": d_ff,
        "p_mma": mma_count(d_m, d["num_heads"]),
        "p_ff":  ff_count(d_m, d_ff),
        "p_norm": norm_count(d_m),
        "p_enc":  encoder_count(d_m, d_ff),
        "p_dec":  decoder_count(d_m, d_ff),
    }
    # enc_dec_params: int = sum([val for val in out.values()])
    # I'm writing this at 12AM, forgive me
    # out["enc-dec params"] = enc_dec_params
    # out["(encoder/decoder) Memory"] = f"""
    #     {round(kb_memory_usage(enc_dec_params, d["quant_bits"]), 2)} Kb"""
    out["(decoder only) Memory"] = f"""
        {round(kb_memory_usage(out["p_dec"], d['quant_bits']), 2)} Kb"""

    # out[f"(encoder/decoder) Memory {d['seq_len']} seq_len"] = f"""
    #     {round(kb_memory_usage(enc_dec_params, d["quant_bits"], 24), 2)} Kb"""
    out[f"(decoder only) Memory {d['seq_len']} seq_len"] = f"""
        {round(kb_memory_usage(out["p_dec"], d['quant_bits'], d['seq_len']), 2)} Kb"""


    return out


def main():
    
    inputs: dict[str, int] = {
        "d_in": 64,
        "d_pos": 5,
        "mul_ff": 4,
        "num_heads": 2,
        "seq_len": 24,
        "quant_bits": 4
    }
    flat_inputs: list[str] = list(inputs.keys())
    while True:
        try:
            print("\n\n","-"*7,"Please Enter the model Dimensions","-"*7)
            print("""
                \nd_in (DEFAULT 64 IP) - Input bits
                \nd_pos (DEFAULT 5) - How many bits to append for psotional encoding 
                \nmul_ff (DEFAULT 4) - d_ff multipler (d_ff = mul * d_model)
                \nnum_heads (DEFAULT 2) - Number of attention heads 
                \nseq_len (DEFAULT 24) - How many previous instructions to attend to 
                \nquant_bits (DEFAULT 4) - How many bits the final model will use per weight
                """)
            print("\nINPUTS:")
            for key in flat_inputs:
                accepted: bool = False
                while not accepted:
                    res = input(f"{key}: ")
                    if not res:
                        print(f"{inputs[key]}")
                        accepted = True
                        continue
                    try:
                        if key == "mul_ff":
                            inputs[key] = float(res)
                        else:
                            inputs[key] = int(res)
                        print(res)
                        accepted = True
                    except ValueError:
                        # Weak validation
                        print("Please enter a valid number")
                        continue
            
            if 2**int(inputs["d_pos"]) < inputs["seq_len"]:
                # 12am error handling
                print("\n\n\nERROR: pos_bits too low for given sequence length! ")
                continue

            results = get_outputs(inputs)
            for key, val in results.items():
                print(f"{key}: {val}")
        except EOFError:
            print("\nExiting..")
            break


if __name__ == "__main__":
    main()