import tensorrt as trt
import pycuda.driver as cuda
# import pycuda.autoinit
import numpy as np
import gc

TRT_LOGGER = trt.Logger(trt.Logger.WARNING)

device_ctx = None

engine_c1 = None
context_c1 = None
bindings_c1 = []
inputs_c1 = []
outputs_c1 = []

engine_c2 = None
context_c2 = None
bindings_c2 = []
inputs_c2 = []
outputs_c2 = []

engine_c3 = None
context_c3 = None
bindings_c3 = []
inputs_c3 = []
outputs_c3 = []


stream_c = None

def load_model():
    global engine_c1, engine_c2, engine_c3, \
           context_c1, context_c2, context_c3, \
           bindings_c1, bindings_c2, bindings_c3, \
           inputs_c1, inputs_c2, inputs_c3, \
           outputs_c1, outputs_c2, outputs_c3, \
           stream_c, device_ctx

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

    cuda.init()
    device = cuda.Device(0)
    device_ctx = device.make_context()
    print("<control.py><load_model> Device context initialized")

    print("<control.py><load_model> Loading engine_c1...")
    engine_c1 = load_engine("/mmc/app/install/APP_1/bin/3RW_controller.trt")
    context_c1 = engine_c1.create_execution_context()
    bindings_c1, inputs_c1, outputs_c1 = setup_io(engine_c1)
    print("<control.py><load_model> 3RW_controller.trt loaded")

    print("<control.py><load_model> Loading engine_c2...")
    engine_c2 = load_engine("/mmc/app/install/APP_1/bin/4RW_controller.trt")
    context_c2 = engine_c2.create_execution_context()
    bindings_c2, inputs_c2, outputs_c2 = setup_io(engine_c2)
    print("<control.py><load_model> 4RW_controller.trt loaded")

    print("<control.py><load_model> Loading engine_c3...")
    engine_c3 = load_engine("/mmc/app/install/APP_1/bin/extended_controller.trt")
    context_c3 = engine_c3.create_execution_context()
    bindings_c3, inputs_c3, outputs_c3 = setup_io(engine_c3)
    print("<control.py><load_model> extended_controller.trt loaded")

    stream_c = cuda.Stream()
    print("<control.py><load_model> pycuda stream initialized.")
    print("<control.py><load_model> All models loaded.")

def control_mode1(states):  # super large angle maneuver for 3 RWs
    global engine_c1, context_c1, bindings_c1, inputs_c1, outputs_c1, stream_c

    print("<control.py><control_mode1> entered control_mode1")
    input_host_c1, input_device_c1 = inputs_c1[0]
    print("<control.py><control_mode1> initialized inputs")
    print("<control.py><control_mode1> state tuple is", states)
    print("<control.py><control_mode1> input_host_c1 is ", input_host_c1)
    print("<control.py><control_mode1> input_device_c1 is ", input_device_c1)
    input_host_c1[:] = np.array(states, dtype=np.float32)
    print("<control.py><control_mode1> numpy input loaded, it is", input_host_c1)

    cuda.memcpy_htod_async(input_device_c1, input_host_c1, stream_c)
    print("<control.py><control_mode1> cuda input loaded")
    device_ctx.push()
    try:
        context_c1.execute_async_v2(bindings=bindings_c1, stream_handle=stream_c.handle)
    finally:
        device_ctx.pop()
    print("<control.py><control_mode1> executed successfully")

    output_data_c1 = []
    for host_mem, device_mem in outputs_c1:
        cuda.memcpy_dtoh_async(host_mem, device_mem, stream_c)
        output_data_c1.append(host_mem)
    stream_c.synchronize()
    print("<control.py><control_mode1> output unpacked")

    alpha = output_data_c1[0]
    beta = output_data_c1[1]
    t_c0 = alpha / (alpha + beta + 1e-7)
    print("<control.py><control_mode1> final result is ", t_c0.tolist())

    return t_c0.tolist()


def control_mode2(states):  # attitude tracker for 4 RWs
    global engine_c2, context_c2, bindings_c2, inputs_c2, outputs_c2, stream_c

    print("<control.py><control_mode2> entered control_mode2")
    input_host_c2, input_device_c2 = inputs_c2[0]
    print("<control.py><control_mode2> initialized inputs")
    print("<control.py><control_mode2> state tuple is", states)
    print("<control.py><control_mode2> input_host_c2 is ", input_host_c2)
    print("<control.py><control_mode2> input_device_c2 is ", input_device_c2)
    input_host_c2[:] = np.array(states, dtype=np.float32)
    print("<control.py><control_mode2> numpy input loaded, it is", input_host_c2)

    cuda.memcpy_htod_async(input_device_c2, input_host_c2, stream_c)
    print("<control.py><control_mode2> cuda input loaded")
    device_ctx.push()
    try:
        context_c2.execute_async_v2(bindings=bindings_c2, stream_handle=stream_c.handle)
    finally:
        device_ctx.pop()
    print("<control.py><control_mode2> executed successfully")

    output_data_c2 = []
    for host_mem, device_mem in outputs_c2:
        cuda.memcpy_dtoh_async(host_mem, device_mem, stream_c)
        output_data_c2.append(host_mem)
    stream_c.synchronize()
    print("<control.py><control_mode2> output unpacked")

    alpha = output_data_c2[0]
    beta = output_data_c2[1]
    t_c0 = alpha / (alpha + beta + 1e-7)
    #print("<control.py><control_mode2> output mapped into [-1,1]")
    print("<control.py><control_mode2> final result is ", t_c0.tolist())

    return t_c0.tolist()

def control_mode3(states):  # Behavior clone controller
    global engine_c3, context_c3, bindings_c3, inputs_c3, outputs_c3, stream_c

    print("<control.py><control_mode3> entered control_mode3")
    input_host_c3, input_device_c3 = inputs_c3[0]
    print("<control.py><control_mode3> initialized inputs")
    print("<control.py><control_mode3> state tuple is", states)
    print("<control.py><control_mode3> input_host_c3 is ", input_host_c3)
    print("<control.py><control_mode3> input_device_c3 is ", input_device_c3)
    input_host_c3[:] = np.array(states, dtype=np.float32)
    print("<control.py><control_mode3> numpy input loaded, it is", input_host_c3)

    cuda.memcpy_htod_async(input_device_c3, input_host_c3, stream_c)
    print("<control.py><control_mode3> cuda input loaded")
    device_ctx.push()
    try:
        context_c3.execute_async_v2(bindings=bindings_c3, stream_handle=stream_c.handle)
    finally:
        device_ctx.pop()
    print("<control.py><control_mode3> executed successfully")

    output_data_c3 = []
    for host_mem, device_mem in outputs_c3:
        cuda.memcpy_dtoh_async(host_mem, device_mem, stream_c)
        output_data_c3.append(host_mem)
    stream_c.synchronize()
    print("<control.py><control_mode3> output unpacked")

    alpha = output_data_c3[0]
    beta = output_data_c3[1]
    t_c0 = alpha / (alpha + beta + 1e-7)
    #print("<control.py><control_mode3> output mapped into [-1,1]")
    print("<control.py><control_mode3> final result is ", t_c0.tolist())

    return t_c0.tolist()

def release():
    global engine_c1, engine_c2, engine_c3, \
        context_c1, context_c2, context_c3, \
        bindings_c1, bindings_c2, bindings_c3, \
        inputs_c1, inputs_c2, inputs_c3, \
        outputs_c1, outputs_c2, outputs_c3, \
        stream_c

    del engine_c1, engine_c2, engine_c3, context_c1, context_c2, context_c3, stream_c

    device_ctx.detach()
    del device_ctx

    bindings_c1.clear()
    bindings_c2.clear()
    bindings_c3.clear()

    inputs_c1.clear()
    inputs_c2.clear()
    inputs_c3.clear()

    outputs_c1.clear()
    outputs_c2.clear()
    outputs_c3.clear()

    gc.collect()
    print("<control.py><release> Intelligent control TensorRT resources released.")
# if __name__ == '__main__':
    # load_model()
    # for i in range (10):
    #     infer("0.bmp")
    # release()
    