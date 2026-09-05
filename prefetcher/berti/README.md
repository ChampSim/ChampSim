# How to run Berti

Ensure the L1D configuration in the `config.json` file looks like this before running Berti:

```json
"L1D": {
    "prefetch_as_load": false,
    "virtual_prefetch": true,
    "prefetch_activate": "LOAD,WRITE,PREFETCH",
    "prefetcher": "berti"
},
```
