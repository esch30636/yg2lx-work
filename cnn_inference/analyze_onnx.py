#!/usr/bin/env python3
"""
Analyze battery_cnn_int8.onnx — print every layer: type, dims, params, weights shape.
"""
import onnx
import numpy as np
import sys

MODEL_PATH = "/home/legion/Downloads/battery_cnn_int8.onnx"

def dtype_str(elem_type):
    MAP = {
        1:  "float32",
        2:  "uint8",
        3:  "int8",
        6:  "int32",
        7:  "int64",
        10: "float16",
    }
    return MAP.get(elem_type, f"type_{elem_type}")

def dim_str(shape):
    if not shape:
        return "scalar"
    return " × ".join(str(d if isinstance(d, int) else "?") for d in shape)

def analyze():
    model = onnx.load(MODEL_PATH)
    graph = model.graph

    print("=" * 72)
    print(f"ONNX Model: {MODEL_PATH}")
    print(f"IR version: {model.ir_version}")
    print(f"Producer:   {model.producer_name}  v{model.producer_version}")
    print(f"Opset:      {model.opset_import[0].domain or 'ai.onnx'} v{model.opset_import[0].version}")
    print()

    # --- Input / Output ---
    print("── INPUT ──")
    for inp in graph.input:
        shape = [d.dim_value for d in inp.type.tensor_type.shape.dim]
        print(f"  {inp.name}:  {dim_str(shape)}  ({dtype_str(inp.type.tensor_type.elem_type)})")

    print("\n── OUTPUT ──")
    for out in graph.output:
        shape = [d.dim_value for d in out.type.tensor_type.shape.dim]
        print(f"  {out.name}:  {dim_str(shape)}  ({dtype_str(out.type.tensor_type.elem_type)})")

    # --- Weights (initializers) ---
    print(f"\n── WEIGHTS ({len(graph.initializer)} tensors) ──")
    total_params = 0
    for w in sorted(graph.initializer, key=lambda x: x.name):
        raw = onnx.numpy_helper.to_array(w)
        total_params += raw.size
        shape_str = " × ".join(str(d) for d in raw.shape)
        dtype = dtype_str(w.data_type)
        print(f"  {w.name:40s}  [{shape_str}]  {dtype}  ={raw.size:,} params")

    print(f"\n  TOTAL: {total_params:,} parameters")

    # --- Compute graph (nodes) ---
    print(f"\n── COMPUTE GRAPH ({len(graph.node)} nodes) ──")
    for i, node in enumerate(graph.node):
        op = node.op_type
        inputs  = ", ".join(node.input[:4])
        outputs = ", ".join(node.output[:4])
        attrs = {}
        for a in node.attribute:
            if a.type == onnx.AttributeProto.INTS:
                attrs[a.name] = list(a.ints)[:8]
            elif a.type == onnx.AttributeProto.INT:
                attrs[a.name] = a.i
            elif a.type == onnx.AttributeProto.FLOAT:
                attrs[a.name] = a.f
            elif a.type == onnx.AttributeProto.STRING:
                attrs[a.name] = a.s.decode()
        attrs_short = ", ".join(f"{k}={v}" for k, v in attrs.items())
        print(f"  [{i:3d}] {op:20s}  in=({inputs})  out=({outputs})" +
              (f"  {{{attrs_short}}}" if attrs else ""))

    # --- Value info (intermediate shapes, if available) ---
    print(f"\n── INTERMEDIATE SHAPES (value_info, {len(graph.value_info)} entries) ──")
    for vi in graph.value_info:
        shape = [d.dim_value for d in vi.type.tensor_type.shape.dim]
        print(f"  {vi.name:40s}  {dim_str(shape)}  ({dtype_str(vi.type.tensor_type.elem_type)})")

    print("\n" + "=" * 72)

    # --- Summary for C code generation ---
    print("\n── C CODE GENERATION SUMMARY ──")
    for inp in graph.input:
        shape = [d.dim_value for d in inp.type.tensor_type.shape.dim]
        print(f"// Input:  {inp.name}  shape={shape}")
    for out in graph.output:
        shape = [d.dim_value for d in out.type.tensor_type.shape.dim]
        print(f"// Output: {out.name}  shape={shape}")

if __name__ == "__main__":
    analyze()
