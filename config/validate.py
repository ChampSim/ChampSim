import itertools

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

core_error_validators = [
    ('ifetch_buffer_size', (is_nonnegative(), is_integral())),
    ('decode_buffer_size', (is_nonnegative(), is_integral())),
    ('dispatch_buffer_size', (is_nonnegative(), is_integral())),
    ('rob_size', (is_nonnegative(), is_integral())),
    ('lq_size', (is_nonnegative(), is_integral())),
    ('sq_size', (is_nonnegative(), is_integral())),
    ('fetch_width', (is_nonnegative(), is_integral())),
    ('decode_width', (is_nonnegative(), is_integral())),
    ('dispatch_width', (is_nonnegative(), is_integral())),
    ('schedule_size', (is_nonnegative(), is_integral())),
    ('execute_width', (is_nonnegative(), is_integral())),
    ('lq_width', (is_nonnegative(), is_integral())),
    ('sq_width', (is_nonnegative(), is_integral())),
    ('retire_width', (is_nonnegative(), is_integral())),
    ('mispredict_penalty', (is_nonnegative(), is_integral())),
    ('decode_latency', (is_nonnegative(), is_integral())),
    ('dispatch_latency', (is_nonnegative(), is_integral())),
    ('schedule_latency', (is_nonnegative(), is_integral())),
    ('execute_latency', (is_nonnegative(), is_integral())),
    ('dib_set', (is_nonnegative(), is_integral())),
    ('dib_way', (is_nonnegative(), is_integral())),
    ('dib_window', (is_nonnegative(), is_integral()))
]

cache_error_validators = [
    ('frequency', (is_nonnegative(), is_integral())),
    ('sets', (is_nonnegative(), is_integral())),
    ('ways', (is_nonnegative(), is_integral())),
    ('rq_size', (is_nonnegative(), is_integral())),
    ('wq_size', (is_nonnegative(), is_integral())),
    ('pq_size', (is_nonnegative(), is_integral())),
    ('mshr_size', (is_nonnegative(), is_integral())),
    ('latency', (is_nonnegative(), is_integral())),
    ('hit_latency', (is_nonnegative(), is_integral())),
    ('fill_latency', (is_nonnegative(), is_integral())),
    ('max_tag_check', (is_nonnegative(), is_integral())),
    ('max_fill', (is_nonnegative(), is_integral()))
]

ptw_error_validators = [
    ("pscl5_set", (is_nonnegative(), is_integral())),
    ("pscl5_way", (is_nonnegative(), is_integral())),
    ("pscl4_set", (is_nonnegative(), is_integral())),
    ("pscl4_way", (is_nonnegative(), is_integral())),
    ("pscl3_set", (is_nonnegative(), is_integral())),
    ("pscl3_way", (is_nonnegative(), is_integral())),
    ("pscl2_set", (is_nonnegative(), is_integral())),
    ("pscl2_way", (is_nonnegative(), is_integral())),
    ("mshr_size", (is_nonnegative(), is_integral())),
    ("max_read", (is_nonnegative(), is_integral())),
    ("max_write", (is_nonnegative(), is_integral()))
]

error_fmtstr = 'Error: key {} in {} {} failed validation "{}" with value {}'

def validate(cores, caches, ptws, pmem, vmem):
    error_responses = []
    for elem, validator in itertools.product(cores, core_error_validators):
        key, val = validator
        error_responses.extend((error_fmtstr.format(key, 'core', elem.get('name'), v, elem.get(key)) for v in val if key in elem and not v(elem.get(key))))

    for elem, validator in itertools.product(caches.values(), cache_error_validators):
        key, val = validator
        error_responses.extend((error_fmtstr.format(key, 'cache', elem.get('name'), v, elem.get(key)) for v in val if key in elem and not v(elem.get(key))))

    for elem, validator in itertools.product(ptws.values(), ptw_error_validators):
        key, val = validator
        error_responses.extend((error_fmtstr.format(key, 'PTW', elem.get('name'), v, elem.get(key)) for v in val if key in elem and not v(elem.get(key))))

    return error_responses

