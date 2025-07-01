import tensorrt as trt
import pycuda.driver as cuda
import pycuda.autoinit
import numpy as np
import gc

TRT_LOGGER = trt.Logger(trt.Logger.WARNING)

engine_1 = None
context_1 = None
bindings_1 = []
inputs_1 = []
outputs_1 = []

engine_2 = None
context_2 = None
bindings_2 = []
inputs_2 = []
outputs_2 = []

engine_3 = None
context_3 = None
bindings_3 = []
inputs_3 = []
outputs_3 = []


stream = None

def load_model():
    global engine_1, engine_2, engine_3, \
           context_1, context_2, context_3, \
           bindings_1, bindings_2, bindings_3, \
           inputs_1, inputs_2, inputs_3, \
           outputs_1, outputs_2, outputs_3, \
           stream

    def load_engine(path):
        with open(path, 'rb') as f, trt.Runtime(TRT_LOGGER) as runtime:
            return runtime.deserialize_cuda_engine(f.read())

    def setup_io(engine):
        bindings = []
        inputs = []
        outputs = []
        for i in range(engine.num_bindings):
            shape = engine.get_binding_shape(i)
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

    # print("Loading control engine_1...")
    # engine_1 = load_engine("control_model_1.engine")
    # context_1 = engine_1.create_execution_context()
    # bindings_1, inputs_1, outputs_1 = setup_io(engine_1)

    print("Loading control engine_2...")
    engine_2 = load_engine("control_model_2.engine")
    context_2 = engine_2.create_execution_context()
    bindings_2, inputs_2, outputs_2 = setup_io(engine_2)

    # print("Loading control engine_3...")
    # engine_3 = load_engine("reinforced_controller.engine")
    # context_3 = engine_3.create_execution_context()
    # bindings_3, inputs_3, outputs_3 = setup_io(engine_3)

    stream = cuda.Stream()
    print("Models loaded.")

def control_mode1(states):  # super large angle maneuver for 3 RWs
    global engine_1, context_1, bindings_1, inputs_1, outputs_1, stream

    input_host_1, input_device_1 = inputs_1[0]
    input_host_1[:] = np.array(states, dtype=np.float32)

    cuda.memcpy_htod_async(input_device_1, input_host_1, stream)
    context_1.execute_async_v2(bindings=bindings_1, stream_handle=stream.handle)

    output_data_1 = []
    for host_mem, device_mem in outputs_1:
        cuda.memcpy_dtoh_async(host_mem, device_mem, stream)
        output_data_1.append(host_mem)
    stream.synchronize()

    alpha = output_data_1[0]
    beta = output_data_1[1]
    t_c = alpha / (alpha + beta)

    return t_c.tolist()


def control_mode2(states):  # attitude tracker for 4 RWs
    global engine_2, context_2, bindings_2, inputs_2, outputs_2, stream

    input_host_2, input_device_2 = inputs_2[0]
    input_host_2[:] = np.array(states, dtype=np.float32)

    cuda.memcpy_htod_async(input_device_2, input_host_2, stream)
    context_2.execute_async_v2(bindings=bindings_2, stream_handle=stream.handle)

    output_data_2 = []
    for host_mem, device_mem in outputs_2:
        cuda.memcpy_dtoh_async(host_mem, device_mem, stream)
        output_data_2.append(host_mem)
    stream.synchronize()

    alpha = output_data_2[0]
    beta = output_data_2[1]
    t_c = alpha / (alpha + beta)

    return t_c.tolist()

def control_mode3(states):  # Behavior clone controller
    global engine_3, context_3, bindings_3, inputs_3, outputs_3, stream

    input_host_3, input_device_3 = inputs_3[0]
    input_host_3[:] = np.array(states, dtype=np.float32)

    cuda.memcpy_htod_async(input_device_3, input_host_3, stream)
    context_3.execute_async_v2(bindings=bindings_3, stream_handle=stream.handle)

    output_data_3 = []
    for host_mem, device_mem in outputs_3:
        cuda.memcpy_dtoh_async(host_mem, device_mem, stream)
        output_data_3.append(host_mem)
    stream.synchronize()

    alpha = output_data_3[0]
    beta = output_data_3[1]
    t_c = alpha / (alpha + beta)

    return t_c.tolist()

def release():
    global engine_1, engine_2, engine_3, \
        context_1, context_2, context_3, \
        bindings_1, bindings_2, bindings_3, \
        inputs_1, inputs_2, inputs_3, \
        outputs_1, outputs_2, outputs_3, \
        stream

    del engine_1, engine_2, engine_3, context_1, context_2, context_3, stream

    bindings_1.clear()
    bindings_2.clear()
    bindings_3.clear()

    inputs_1.clear()
    inputs_2.clear()
    inputs_3.clear()

    outputs_1.clear()
    outputs_2.clear()
    outputs_3.clear()

    gc.collect()
    print("Intelligent control TensorRT resources released.")

# if __name__ == '__main__':
    # load_model()
    # for i in range (10):
    #     infer("0.bmp")
    # release()
    