import itertools
import math

class is_nonnegative:
    def __call__(self, val):
        return val >= 0

    def __str__(self):
        return 'is nonnegative'

class is_integral:
    def __call__(self, val):
        return int(val) == val

    def __str__(self):
        return 'is integral'

class is_power_of_two:
    def __call__(self, val):
        return val.bit_count() == 1

    def __str__(self):
        return 'is power of two'

class matches_type:
    def __init__(self, t):
        self.type = t

    def __call__(self, val):
        return isinstance(val, self.type)

    def __str__(self):
        return 'has type '+str(self.type)

class key_validator:
    def __init__(self, key, validator):
        self.key = key
        self.validator = validator

    def __call__(self, elem):
        return (self.key not in elem) or (self.validator(elem.get(self.key)))

    def __str__(self):
        return str(self.validator) + ' for key \'' + str(self.key) + '\''

core_error_validators = [
    key_validator('ifetch_buffer_size', is_nonnegative()),
    key_validator('ifetch_buffer_size', is_integral()),
    key_validator('decode_buffer_size', is_nonnegative()),
    key_validator('decode_buffer_size', is_integral()),
    key_validator('dispatch_buffer_size', is_nonnegative()),
    key_validator('dispatch_buffer_size', is_integral()),
    key_validator('rob_size', is_nonnegative()),
    key_validator('rob_size', is_integral()),
    key_validator('lq_size', is_nonnegative()),
    key_validator('lq_size', is_integral()),
    key_validator('sq_size', is_nonnegative()),
    key_validator('sq_size', is_integral()),
    key_validator('fetch_width', is_nonnegative()),
    key_validator('fetch_width', is_integral()),
    key_validator('decode_width', is_nonnegative()),
    key_validator('decode_width', is_integral()),
    key_validator('dispatch_width', is_nonnegative()),
    key_validator('dispatch_width', is_integral()),
    key_validator('schedule_size', is_nonnegative()),
    key_validator('schedule_size', is_integral()),
    key_validator('execute_width', is_nonnegative()),
    key_validator('execute_width', is_integral()),
    key_validator('lq_width', is_nonnegative()),
    key_validator('lq_width', is_integral()),
    key_validator('sq_width', is_nonnegative()),
    key_validator('sq_width', is_integral()),
    key_validator('retire_width', is_nonnegative()),
    key_validator('retire_width', is_integral()),
    key_validator('mispredict_penalty', is_nonnegative()),
    key_validator('mispredict_penalty', is_integral()),
    key_validator('decode_latency', is_nonnegative()),
    key_validator('decode_latency', is_integral()),
    key_validator('dispatch_latency', is_nonnegative()),
    key_validator('dispatch_latency', is_integral()),
    key_validator('schedule_latency', is_nonnegative()),
    key_validator('schedule_latency', is_integral()),
    key_validator('execute_latency', is_nonnegative()),
    key_validator('execute_latency', is_integral()),
    key_validator('dib_set', is_nonnegative()),
    key_validator('dib_set', is_integral()),
    key_validator('dib_set', is_power_of_two()),
    key_validator('dib_way', is_nonnegative()),
    key_validator('dib_way', is_integral()),
    key_validator('dib_window', is_nonnegative()),
    key_validator('dib_window', is_integral()),
    key_validator('dib_window', is_power_of_two()),
    key_validator('branch_predictor', matches_type((str, list))),
    key_validator('btb', matches_type((str, list)))
]

cache_error_validators = [
    key_validator('frequency', is_nonnegative()),
    key_validator('frequency', is_integral()),
    key_validator('sets', is_nonnegative()),
    key_validator('sets', is_integral()),
    key_validator('sets', is_power_of_two()),
    key_validator('ways', is_nonnegative()),
    key_validator('ways', is_integral()),
    key_validator('rq_size', is_nonnegative()),
    key_validator('rq_size', is_integral()),
    key_validator('wq_size', is_nonnegative()),
    key_validator('wq_size', is_integral()),
    key_validator('pq_size', is_nonnegative()),
    key_validator('pq_size', is_integral()),
    key_validator('mshr_size', is_nonnegative()),
    key_validator('mshr_size', is_integral()),
    key_validator('latency', is_nonnegative()),
    key_validator('latency', is_integral()),
    key_validator('hit_latency', is_nonnegative()),
    key_validator('hit_latency', is_integral()),
    key_validator('fill_latency', is_nonnegative()),
    key_validator('fill_latency', is_integral()),
    key_validator('max_tag_check', is_nonnegative()),
    key_validator('max_tag_check', is_integral()),
    key_validator('max_fill', is_nonnegative()),
    key_validator('max_fill', is_integral()),
    key_validator('prefetcher', matches_type((str, list))),
    key_validator('replacement', matches_type((str, list)))
]

ptw_error_validators = [
    key_validator("pscl5_set", is_nonnegative()),
    key_validator("pscl5_set", is_integral()),
    key_validator('pscl5_set', is_power_of_two()),
    key_validator("pscl5_way", is_nonnegative()),
    key_validator("pscl5_way", is_integral()),
    key_validator("pscl4_set", is_nonnegative()),
    key_validator("pscl4_set", is_integral()),
    key_validator('pscl4_set', is_power_of_two()),
    key_validator("pscl4_way", is_nonnegative()),
    key_validator("pscl4_way", is_integral()),
    key_validator("pscl3_set", is_nonnegative()),
    key_validator("pscl3_set", is_integral()),
    key_validator('pscl3_set', is_power_of_two()),
    key_validator("pscl3_way", is_nonnegative()),
    key_validator("pscl3_way", is_integral()),
    key_validator("pscl2_set", is_nonnegative()),
    key_validator("pscl2_set", is_integral()),
    key_validator('pscl2_set', is_power_of_two()),
    key_validator("pscl2_way", is_nonnegative()),
    key_validator("pscl2_way", is_integral()),
    key_validator("mshr_size", is_nonnegative()),
    key_validator("mshr_size", is_integral()),
    key_validator("max_read", is_nonnegative()),
    key_validator("max_read", is_integral()),
    key_validator("max_write", is_nonnegative()),
    key_validator("max_write", is_integral())
]

error_fmtstr = 'Error: {} {} failed validation "{}"'

def validate(cores, caches, ptws, pmem, vmem):
    validators = itertools.chain(
        itertools.product(('core',), cores, core_error_validators),
        itertools.product(('cache',), caches.values(), cache_error_validators),
        itertools.product(('PTW',), ptws.values(), ptw_error_validators)
    )

    error_responses = [error_fmtstr.format(label, elem.get('name'), validator) for label, elem, validator in validators if not validator(elem)]

    return error_responses

