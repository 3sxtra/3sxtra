import json

path = "src/lua/3rd_training_lua-main/data/sfiii3nr1/framedata/@ryu_framedata.json"
with open(path, "r") as f:
    data = json.load(f)

for k, v in data.items():
    if k in ["2304", "1984", "1c34"]:
        print(f"{k} -> {v.get('name')}")
