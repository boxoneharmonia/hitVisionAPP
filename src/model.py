import sys
sys.path.append('/home/user/.local/lib/python3.8/site-packages')

import tensorrt as trt
import pycuda.driver as cuda
import numpy as np
from PIL import Image
import time
import gc
import os.path as osp

TRT_LOGGER = trt.Logger(trt.Logger.WARNING)

cuda_context = None
engine_1 = None
context_1 = None
bindings_1 = []
inputs_1 = []
outputs_1 = []
stream = None

def load_engine(path):
    with open(path, 'rb') as f, trt.Runtime(TRT_LOGGER) as runtime:
        return runtime.deserialize_cuda_engine(f.read())

def setup_io(engine):
    bindings = []
    inputs = []
    outputs = []
    for i in range(engine.num_bindings):
        shape = engine.get_binding_shape(i)
        if -1 in shape:
            shape = [1 if dim == -1 else dim for dim in shape]
        dtype = np.float32
        size = trt.volume(shape)
        host_mem = cuda.pagelocked_empty(size, dtype)
        device_mem = cuda.mem_alloc(host_mem.nbytes)
        bindings.append(int(device_mem))
        if engine.binding_is_input(i):
            inputs.append((host_mem, device_mem))
        else:
            outputs.append((host_mem, device_mem))
    return bindings, inputs, outputs

def load_model():

    global engine_1, context_1, bindings_1, inputs_1, outputs_1, stream, cuda_context

    cuda.init()
    device = cuda.Device(0)
    cuda_context = device.make_context()

    engine_1_path = "/mmc/app/install/APP_1/bin/model_1.engine"
    if not osp.exists(engine_1_path):
        engine_1_path = "model_1.engine"

    print(f"Loading engine_1 from: {engine_1_path}")
    engine_1 = load_engine(engine_1_path)
    context_1 = engine_1.create_execution_context()
    bindings_1, inputs_1, outputs_1 = setup_io(engine_1)
    stream = cuda.Stream()
    print("Models loaded.")
        
def preprocess(image, size=256):
    try:
        resample = Image.Resampling.LANCZOS  # Pillow ≥ 9.1.0
    except AttributeError:
        resample = Image.LANCZOS             # Older versions fallback
    image = image.resize((size, size), resample)
    image = np.array(image).transpose((2, 0, 1)).astype(np.float32) / 255.0
    return np.expand_dims(image, axis=0)  # Add batch dim, shape [1, 3, 256, 256]

def crop_roi(image, bbox, size=384):
    # bbox: [x1, y1, x2, y2] in normalized coords
    w, h = image.size
    x1, y1, x2, y2 = bbox
    x1 = max(0, min(1, x1))
    y1 = max(0, min(1, y1))
    x2 = max(0, min(1, x2))
    y2 = max(0, min(1, y2))
    left = int(x1 * w)
    top = int(y1 * h)
    right = int(x2 * w)
    bottom = int(y2 * h)
    roi = image.crop((left, top, right, bottom)).resize((size, size), Image.LANCZOS)
    roi = np.array(roi).transpose((2, 0, 1)).astype(np.float32) / 255.0
    return np.expand_dims(roi, axis=0)

def infer_1(path):
    global engine_1, context_1, bindings_1, inputs_1, outputs_1, stream

    image = Image.open(path).convert("RGB")
    resized = preprocess(image, size=256)
    context_1.set_binding_shape(0, resized.shape)

    start = time.time()

    # -------- Inference 1 --------
    input_host_1, input_device_1 = inputs_1[0]
    input_host_1[:] = resized.ravel()

    cuda.memcpy_htod_async(input_device_1, input_host_1, stream)
    context_1.execute_async_v2(bindings=bindings_1, stream_handle=stream.handle)

    output_data_1 = []
    for host_mem, device_mem in outputs_1:
        cuda.memcpy_dtoh_async(host_mem, device_mem, stream)
        output_data_1.append(host_mem)
    stream.synchronize()

    confidence = float(output_data_1[0].squeeze())
    bbox = (output_data_1[1].reshape(-1) / 256.0).tolist()
    tmc = (output_data_1[2].reshape(-1)).tolist()

    elapsed = time.time() - start
    bbox_str = [f'"{x:.4f}"' for x in bbox]
    tmc_str = [f'"{x:.4f}"' for x in tmc]

    print(f'{path}: {elapsed*1000:.2f} ms, confidence: {confidence:.4f}, bbox: {bbox_str}, tmc: {tmc_str}')

    return confidence, bbox, tmc

def release():
    global engine_1, context_1, bindings_1, inputs_1, outputs_1, stream, cuda_context
    del engine_1, context_1, stream
    cuda_context.detach()
    del cuda_context
    bindings_1.clear()
    inputs_1.clear()
    outputs_1.clear()
    gc.collect()
    print("TensorRT resources released.")

if __name__ == '__main__':
    print("model.py loaded.")
    # load_model()
    # for i in range (10):
    #     infer("0.bmp")
    # release()
    