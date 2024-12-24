#! usr/bin/env/python3

import json,os,subprocess,argparse

PADDED_PATH = "build/bench"
UNPADDED_PATH = "unpadded"
DELIM = ","
NULL_ARG = "NULL"

def strerror(message:str):
    print(message)
    exit(1)


"""Utilitiies for QueueFilter parsing"""

QUEUES_HASH = {
    "LinkedCRQ" : "LinkedCRQueue",
    "LCRQ"      : "LinkedCRQueue",
    "LinkedPRQ" : "LinkedPRQueue",
    "LinkedPRQueue" : "LinkedPRQueue",
    "LPRQ"      : "LinkedPRQueue",
    "BoundedCRQ": "BoundedCRQueue",
    "BCRQ"      : "BoundedCRQueue",
    "BoundedCRQueue": "BoundedCRQueue",
    "BoundedPRQ": "BoundedPRQueue",
    "BPRQ"      : "BoundedPRQueue",
    "BoundedPRQueue": "BoundedPRQueue",
    "LinkedMutexQueue": "LinkedMuxQueue",
    "LMQ"       : "LinkedMuxQueue",
    "LinkedMuxQueue": "LinkedMuxQueue",
    "BoundedMutexQueue": "BoundedMuxQueue",
    "BMQ"       : "BoundedMuxQueue",
    "BoundedMuxQueue": "BoundedMuxQueue",
    "FAAQueue"  : "FAAQueue",
    "FAAQ"      : "FAAQueue",
    "FAA"       : "FAAQueue"
}
BOUNDED     : set = {"BoundedCRQueue", "BoundedPRQueue", "BoundedMuxQueue"}
UNBOUNDED   : set = {"LinkedCRQueue", "LinkedPRQueue", "LinkedMuxQueue", "FAAQueue"}
QUEUES      : set = BOUNDED.union(UNBOUNDED)

def parseQueues(data):
    filter = set()
    if type(data) is str: data = [data]
    if type(data) is list and any(type(i) is not str for i in data):
        strerror(f"Invalid queue filter type: {i} | Must be str")
    elif type(data) is not list:
        strerror(f"Invalid queue filter type: {data} | Must be list")
    for queue in data:
        match queue:
            case "All": filter = filter.union(QUEUES)
            case "Bounded": filter = filter.union(BOUNDED)
            case "Unbounded": filter = filter.union(UNBOUNDED)
            case _: 
                q = QUEUES_HASH.get(queue)
                filter.add(q) if q is not None else strerror(f"Invalid queue filter: {data}")

    return DELIM.join(list(filter))

"""Utilities for flag parsing"""

BENCH_FLAGS = ["output","progress","overwrite"]

def parseFlags(data):
    if type(data) is str: data = list(data)
    if type(data) is not list: strerror(f"Invalid flag type: {data}")

    flags = [False,False,False]

    for key in data:
        if type(key) is not str: strerror(f"Invalid flag type: {key}: Parsing flags")
        if key in BENCH_FLAGS:
            flags[BENCH_FLAGS.index(key)] = True
        else:
            strerror(f"Invalid flag: {key}: Parsing flags")
    
    return DELIM.join(list(map(lambda x: "1" if x else "0",flags)))

"""Memory Flags parsing
1. Threshold: can set max_memoy,min_memory,supSleep,infSleep | if not min_memory set to 0 if not max_memory set to 2^64
2. Producer Control: if one flag is set then all must be set
3. Consumer Control: if one flag is set then all must be set
"""
MEMORY_THRESH_FLAGS = ["max_memory","min_memory","supSleep","infSleep"]
MEM_CTRL_PROD       = ["prodInitialDelay","prodSleep","prodUptime"]
MEM_CTRL_CONS       = ["consInitialDelay","consSleep","consUptime"]

def parseMemoryFlags(data):
    mem_thresh = [-1] * len(MEMORY_THRESH_FLAGS)
    mem_prod_control = [-1] * len(MEM_CTRL_PROD)
    mem_cons_control = [-1] * len(MEM_CTRL_CONS)
    # Check flags types
    for key in data.keys():
        key = data[key]
        if type(key) is not int:
            strerror(f"Invalid Memory Argument Type: {key} | Must be int") 
        elif key < 0:
            strerror(f"Invalid Memory Argument: {key} | Must be positive int")

    for key in list(data):
        # if key in ThreshFlags
        if key in MEMORY_THRESH_FLAGS:
            mem_thresh[MEMORY_THRESH_FLAGS.index(key)] = data[key]
        # if key in ProdControl
        elif key in MEM_CTRL_PROD:
            if all(data.get(arg) is not None for arg in MEM_CTRL_PROD):
                for p_arg in MEM_CTRL_PROD:
                    mem_prod_control[MEM_CTRL_PROD.index(p_arg)] = data[p_arg]
                    del data[p_arg]  # Remove processed keys
            else:
                missing = [arg for arg in MEM_CTRL_PROD if data.get(arg) is None]
                strerror(f"If one Producer Control is set, all must be set\n\tMissing: {', '.join(missing)}")
        # if key in ConsControl
        elif key in MEM_CTRL_CONS:
            if all(data.get(arg) is not None for arg in MEM_CTRL_CONS):
                for c_arg in MEM_CTRL_CONS:
                    mem_cons_control[MEM_CTRL_CONS.index(c_arg)] = data[c_arg]
                    del data[c_arg]
            else:
                missing = [arg for arg in MEM_CTRL_CONS if data.get(arg) is None]
                strerror(f"If one Consumer Control is set, all must be set\n\tMissing: {', '.join(missing)}")
        # Invalid Memory Argument
        else:
            strerror(f"Invalid Memory Argument: {key}")
    
    return DELIM.join(list(mem_thresh+mem_prod_control+mem_cons_control))

""" Benchmark JSON Parsing"""

BENCH_ID            = {"EnqueueDequeue", "ProdCons", "Pairs", "Memory"}
REQUIRED            = {"benchmark","file","threads","runs","iterations","queues"}
REQUIRED_PROD_CONS  = {"producers","consumers"} #if thread not set
REQUIRED_MEM        = {"benchmark","file","threads","queues","duration_sec","granularity_msec"}
OPTIONAL            = {"warmup","additionalWork","flags"}
#union to optional
OPTIONAL_MEM        = {"memoryArgs"}
OPTIONAL_PROD_CONS  = {"balanced"}

"""
{
    benchmark: "EnqueueDequeue" | "ProdCons" | "Pairs" | "Memory",
    padding : bool,
    file    : str, 
    threads : int, list int
    producers | consumers: int, list int #if ProdCons sobstituted to 'threads'
    runs    : int, list int
    iterations : int, list int
    queues  : str, list str
    warmup  : int, list int | nullable
    balanced : bool | nullable [False]
    additionalWork: float, list float | nullable [set 0]
    flags   : subset{"output","progress","overwrite"} str | list str
    memoryArgs : dict #if MemoryBenchmark
}

NOTE:
1.  [ProdCons] If 'threads' is set, 'producers' and 'consumers' mustn't exist [they take the value of 'threads']
    else both 'producers' and 'consumers' must be set and 'threads' mustn't exist
2.  [Memory] 'runs' and 'iterations' are substituted with 'duration_sec' and 'granularity_msec'
    (they take the same values)


RETURN: list of arguments to be passed to the C++ program
["benchmark","queues","file"] --> 1,2,3
["runs","iterations"] if id != "Memory" else ["duration_sec","granularity_msec"] --> 4,5
["warmup","additionalWork","flags"] --> 6,7,8
["memoryArgs"] if id == "Memory" else (["balanced"]) --> 9
["threads"] if id != "ProdCons" else ["producers","consumers"] --> 10 or 10,11
"""
def parseBenchmark(benchmark:dict) -> list :
    #remove null keys
    benchmark = {k: v for k, v in benchmark.items() if v is not None}
    id = benchmark.get("benchmark")
    if id is None:
        strerror("Missing Benchmark Type")
    elif id not in BENCH_ID:
        strerror(f"Invalid Benchmark Type: {id}")

    #Required and Optional Keys
    required = REQUIRED
    optional = OPTIONAL
    
    #Fix for ProdCons Benchmark
    if id == "ProdCons":
        required = REQUIRED.difference({"threads"}).union(REQUIRED_PROD_CONS)
        optional = optional.union(OPTIONAL_PROD_CONS)
        threads, producers, consumers = map(benchmark.get, ["threads", "producers", "consumers"])
        if threads is not None:
            if producers is None and consumers is None:
                benchmark["producers"],benchmark["consumers"] = threads,threads
                del benchmark["threads"] #Remove threads
            else:
                strerror("Invalid Thread Configuration\n\tAlternative: set 'threads' for both or 'producers' and 'consumers'")
        elif producers is None or consumers is None:
            strerror("Missing Producer and/or Consumer Threads\n\tAlternative can set 'threads' for both")

    #Fix for Memory Benchmark
    elif id == "Memory":
        required = REQUIRED_MEM
        optional = optional.union(OPTIONAL_MEM)
    
    #Check for invalid keys
    for key in benchmark.keys():
        if key not in required.union(optional):strerror(f"Invalid Key: {key}")

    for key in required:
        if benchmark.get(key) is None: strerror(f"Missing Required Key: {key}")
        match key:
            case "threads" |"producers"|"consumers"| "runs" | "iterations" | "granularity_msec" | "duration_sec" | "warmup":
                arg = benchmark[key]
                if type(arg) is int: arg = [arg]
                #Check for uniform type and positive values
                if type(arg) is not list or any((type(i) is not int or i <= 0) for i in arg):
                    strerror(f"Invalid {key} type: {arg}")
                benchmark[key] = DELIM.join(list(map(str,arg)))
            case "queues":
                benchmark[key] = parseQueues(benchmark[key])
            case "file":
                if os.path.exists(benchmark[key]) and not os.access(benchmark[key],os.R_OK | os.R_OK):
                    strerror(f"File {benchmark[key]} does not have permissions (Read and/or Write)")
    
    for key in optional:
        if benchmark.get(key) is None:
            benchmark[key] = NULL_ARG
        else:
            match key:
                case "flags":
                    benchmark[key] = parseFlags(benchmark[key])
                case "memoryArgs":
                    benchmark[key] = parseMemoryFlags(benchmark[key])
                case "warmup":
                    arg = benchmark[key]
                    if type(arg) is int: arg = [arg]
                    #Check for uniform type and positive values
                    if type(arg) is not list or any((type(i) is not int or i <= 0) for i in arg):
                        strerror(f"Invalid {key} value: {arg}")
                    benchmark[key] = DELIM.join(list(map(str,arg)))
                case "additionalWork":
                    arg = benchmark[key]
                    if type(arg) in {float,int}: arg = [arg]
                    #Check for uniform type and positive values
                    if type(arg) is not list or any((type(i) not in {float,int} or i < 0) for i in arg):
                        strerror(f"Invalid {key} value: {arg}")
                    benchmark[key] = DELIM.join(list(map(str,arg)))
                case "balanced":
                    if type(benchmark[key]) is not bool:
                        strerror(f"Invalid {key} value: {benchmark[key]}")
                    benchmark[key] = "1" if benchmark[key] else "0"
    
    #This order to make easier to parse arguments in the c++ program
    parsed  =   ["benchmark","queues","file"] 
    parsed  +=  ["runs","iterations"] if id != "Memory" else ["duration_sec","granularity_msec"]
    parsed  +=  ["warmup","additionalWork","flags"]
    parsed  +=  ["memoryArgs"] if id == "Memory" else (["balanced"] if id == "ProdCons" else [])
    #different arguments between different benchmarks at the end
    parsed  +=  ["threads"] if id != "ProdCons" else ["producers","consumers"]

    return [f"{arg}:{benchmark[arg]}" for arg in parsed]

def scheduleProcess(jsonArgs:dict):
    process = PADDED_PATH if jsonArgs.get('padding') is not False else UNPADDED_PATH
    try: 
        del jsonArgs['padding'] 
    except KeyError: pass
    
    args = parseBenchmark(jsonArgs)
    print(args)
    #DEBUG:
    #subprocess.run(process + args)

#Arguments can be given via command line or json file (can contain list of benchmarks or singles)
def main(data):
    if isinstance(data,dict):    
        scheduleProcess(data)
    elif isinstance(data,list):
        for i in data:
            scheduleProcess(i)
if __name__ == "__main__":
    class CustomHelpFormatter(argparse.HelpFormatter):
        def _format_action_invocation(self, action):
            if action.nargs and action.option_strings:
                return ', '.join(action.option_strings)
            return super()._format_action_invocation(action)

    parser = argparse.ArgumentParser(description="Process benchmarking arguments",formatter_class=CustomHelpFormatter,
                                    usage="""%(prog)s benchmark_id output_file [-h] [-t THREADS] [-q QUEUES] [-r RUNS] [-i ITER] [-s SIZE] [-w WARMUP] [-a ADDITIONAL_WORK] [-p]""",)

    positional = parser.add_argument_group  ("Positional Arguments")
    queueOpt = parser.add_argument_group    ("Queue Options")
    execCtrl = parser.add_argument_group    ("Execution Control")
    benchConf = parser.add_argument_group   ("Benchmark Configuration")

    benchConf.add_argument  ("-j","--json_file",help="json file",type=str,metavar="",default=None)
    args = parser.parse_args()
    if args.json_file is None:
        #this only if the arguments are given via command line (disable if json)
        positional.add_argument ("benchmark",help="(Required) benchmark ID",type=str)
        positional.add_argument ("csv", type=str, help="(Required) csv output file")

        benchConf.add_argument  ("-t","--threads",help="(Required) threads [<= 128]",nargs="+",type=int,metavar="")
        queueOpt.add_argument   ("-q","--queue",help="(Required) queues [filters: 'Bounded','Unbounded','All']",nargs="+",type=str,metavar="")
        execCtrl.add_argument   ("-r","--runs",help="(Required) Repetition of the same test",type=int,nargs="+",metavar="")
        execCtrl.add_argument   ("-i","--iter",help="(Required) Iteration|Duration",type=int,nargs="+",metavar="")
        queueOpt.add_argument   ("-s","--size",help="queue sizes",type=int,nargs="+",default=None,metavar="")
        benchConf.add_argument  ("-w","--warmup",help="Warmup iterations",type=int,nargs="+",default=None,metavar="")
        benchConf.add_argument  ("-a","--additionalWork",help="Work done between operations",type=float,nargs="+",default=None,metavar='')
        queueOpt.add_argument   ("-p","--disable_padding",help="disable padding for queues",action="store_true")
    args = parser.parse_args()

    #make a dictionary
    data = None
    if(args.json_file is not None):
        data = None
        with open(args.json_file,'r') as f:
            data = json.load(f)
    else:
        data = {
            "benchmark":args.benchmark,
            "file":args.csv,
            "threads":args.threads,
            "queue":args.queue,
            "size":args.size,
            "warmup":args.warmup,
            "additionalWork":args.additionalWork,
            "padding":args.disable_padding
        }
        
        r,i = "runs","iterations" if args.benchmark != "Memory" else "duration_sec","granularity_msec"
        data[r],data[i] = args.runs,args.iter

    #remove None values
    main(data)
    
    

        
