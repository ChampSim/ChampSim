import os
import re
import pandas as pd

results_folder = "results_10M"
data = []

for filename in os.listdir(results_folder):
    if filename.endswith(".txt"):
        filepath = os.path.join(results_folder, filename)

        with open(filepath, "r") as f:
            text = f.read()

            # Extract total accesses
            l1d_match = re.search(r"L1D TOTAL\s+ACCESS:\s+(\d+)", text)
            l1i_match = re.search(r"L1I TOTAL\s+ACCESS:\s+(\d+)", text)
            l2c_match = re.search(r"L2C TOTAL\s+ACCESS:\s+(\d+)", text)
            llc_match = re.search(r"LLC TOTAL\s+ACCESS:\s+(\d+)", text)

            if l1d_match and l1i_match and l2c_match and llc_match:
                l1d = int(l1d_match.group(1))
                l1i = int(l1i_match.group(1))
                l2c = int(l2c_match.group(1))
                llc = int(llc_match.group(1))

                total_accesses = l1d + l1i + l2c + llc
                llc_access_percent = (llc / total_accesses) * 100 if total_accesses > 0 else 0

                data.append({
                    "File": filename,
                    "L1D_TOTAL": l1d,
                    "L1I_TOTAL": l1i,
                    "L2C_TOTAL": l2c,
                    "LLC_TOTAL": llc,
                    "LLC_Access_%": round(llc_access_percent, 4)
                })

# Convert to DataFrame
df = pd.DataFrame(data)

# Compute average LLC Access %
average_llc = df["LLC_Access_%"].mean()
df.loc[len(df)] = {
    "File": "AVERAGE",
    "L1D_TOTAL": df["L1D_TOTAL"].sum(),
    "L1I_TOTAL": df["L1I_TOTAL"].sum(),
    "L2C_TOTAL": df["L2C_TOTAL"].sum(),
    "LLC_TOTAL": df["LLC_TOTAL"].sum(),
    "LLC_Access_%": round(average_llc, 4)
}

# Save to LibreOffice Calc format (.ods)
output_file = "LLC_Access_Percentages.ods"
df.to_excel(output_file, index=False, engine="odf")

print(f"✅ Done! Saved results to {output_file}")
