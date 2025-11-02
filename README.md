# `serveperf`

Inference service profiler for OAI API servers in pure C. Automatically adjusts the request rate
to meet the desired amount by enforcing wait times on worker threads.

# Example:

```bash
./serveperf --req-rate 100,300 --endpoint https://api.openai.com/v1/completions --input-len 1000 --num-requests 200 --model gpt-3.5-turbo-instruct --text-url https://norvig.com/big.txt

reqs: 189.000000, req rate: 100, average throughput: 46.318343, average TTFT: 0.562157
req rate: 100, request throughput: 5.685114
reqs: 186.000000, req rate: 300, average throughput: 43.809200, average TTFT: 0.699803
req rate: 300, request throughput: 25.471062
```

A weekend project and not intended for production use. `--text-url` assumes a
raw .txt file over HTTP like the one given in the example.