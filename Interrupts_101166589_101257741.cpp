/**
 * @file interrupts.cpp
 * Implementation of OS Simulator - Part 3 and Part 4
 */

#include "Interrupts_101166589_101257741.hpp"

std::vector<Partition> partition_table;
std::vector<PCB> pcb_table;
std::vector<ExternalFile> external_files;
std::queue<int> ready_queue;
std::map<int, std::vector<int>> parent_child_map;

int next_pid = 1;

void initialize_system() {
    partition_table = {
        {0, 40, "free"},
        {1, 25, "free"},
        {2, 15, "free"},
        {3, 10, "free"},
        {4, 8, "free"},
        {5, 2, "free"}
    };
    
    PCB init_process;
    init_process.pid = 0;
    init_process.ppid = -1;
    init_process.program_name = "init";
    init_process.partition_number = 6;
    init_process.size = 1;
    init_process.state = "running";
    init_process.priority = 0;
    
    pcb_table.push_back(init_process);
    ready_queue.push(0);
    next_pid = 1;
}

std::vector<ExternalFile> load_external_files(const std::string& filename) {
    std::vector<ExternalFile> files;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "Warning: Could not open " << filename << std::endl;
        return files;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        auto parts = split_delim(line, ",");
        if (parts.size() == 2) {
            ExternalFile ext_file;
            ext_file.program_name = parts[0];
            ext_file.size = std::stoi(parts[1]);
            files.push_back(ext_file);
        }
    }
    file.close();
    return files;
}

int find_available_partition(unsigned int program_size) {
    for (auto& part : partition_table) {
        if (part.code == "free" && part.size >= program_size) {
            return part.number;
        }
    }
    return -1;
}

void add_to_ready_queue(int pid) {
    ready_queue.push(pid);
}

int get_next_process() {
    if (ready_queue.empty()) return -1;
    int next = ready_queue.front();
    ready_queue.pop();
    return next;
}

void remove_from_ready_queue(int pid) {
    std::queue<int> temp;
    while (!ready_queue.empty()) {
        int current = ready_queue.front();
        ready_queue.pop();
        if (current != pid) {
            temp.push(current);
        }
    }
    ready_queue = temp;
}

bool is_child_of(int pid, int parent_pid) {
    for (auto& pcb : pcb_table) {
        if (pcb.pid == pid && pcb.ppid == parent_pid) {
            return true;
        }
    }
    return false;
}

void terminate_process(int pid) {
    for (auto& pcb : pcb_table) {
        if (pcb.pid == pid) {
            pcb.state = "terminated";
            break;
        }
    }
}

std::string handle_fork(int& current_time, std::vector<std::string>& vectors, int current_pid) {
    std::string result = "";
    const int CONTEXT_TIME = 10;
    
    auto [boilerplate, new_time] = intr_boilerplate(current_time, 2, CONTEXT_TIME, vectors);
    result += boilerplate;
    current_time = new_time;
    
    PCB parent;
    bool found = false;
    for (const auto& pcb : pcb_table) {
        if (pcb.pid == current_pid) {
            parent = pcb;
            found = true;
            break;
        }
    }
    
    if (!found) {
        result += std::to_string(current_time) + ", 1, ERROR: Parent not found\n";
        current_time += 1;
        result += execute_iret(current_time);
        return result;
    }
    
    PCB child = parent;
    child.pid = next_pid++;
    child.ppid = current_pid;
    child.priority = 1;
    child.partition_number = 6;
    child.size = 1;
    pcb_table.push_back(child);
    parent_child_map[current_pid].push_back(child.pid);
    add_to_ready_queue(child.pid);
    
    result += std::to_string(current_time) + ", 10, cloning the PCB\n";
    current_time += 10;
    
    result += std::to_string(current_time) + ", 0, scheduler called\n";
    
    result += execute_iret(current_time);
    
    return result;
}

std::string handle_exec(const std::string& program_name, int trace_duration, int& current_time,
                        std::vector<std::string>& vectors, 
                        std::vector<ExternalFile>& external_files,
                        int current_pid) {
    std::string result = "";
    const int CONTEXT_TIME = 10;
    
    auto [boilerplate, new_time] = intr_boilerplate(current_time, 3, CONTEXT_TIME, vectors);
    result += boilerplate;
    current_time = new_time;
    
    int program_size = 0;
    bool found = false;
    for (const auto& file : external_files) {
        if (file.program_name == program_name) {
            program_size = file.size;
            found = true;
            break;
        }
    }
    
    if (!found) {
        result += std::to_string(current_time) + ", 1, ERROR: Program not found\n";
        current_time += 1;
        result += execute_iret(current_time);
        result += switch_to_user_mode(current_time);
        return result;
    }
    
    result += std::to_string(current_time) + ", " + std::to_string(trace_duration) 
            + ", Program is " + std::to_string(program_size) + " MB large\n";
    current_time += trace_duration;
    int partition_to_use = find_available_partition(program_size);
    
    if (partition_to_use == -1) {
        result += std::to_string(current_time) + ", 1, ERROR: No partition\n";
        current_time += 1;
        result += execute_iret(current_time);
        result += switch_to_user_mode(current_time);
        return result;
    }
    
    for (auto& part : partition_table) {
        if (part.number == partition_to_use) {
            part.code = program_name;
            break;
        }
    }
    
    int loader_time = program_size * 15;
    result += std::to_string(current_time) + ", " + std::to_string(loader_time)
            + ", loading " + program_name + " from disk to partition " 
            + std::to_string(partition_to_use) + "\n";
    current_time += loader_time;
    
    result += std::to_string(current_time) + ", 3, marking partition as occupied\n";
    current_time += 3;
    
    result += std::to_string(current_time) + ", 6, updating PCB\n";
    current_time += 6;
    
    for (auto& pcb : pcb_table) {
        if (pcb.pid == current_pid) {
            pcb.program_name = program_name;
            pcb.partition_number = partition_to_use;
            pcb.size = program_size;
            break;
        }
    }
    
    result += std::to_string(current_time) + ", 0, scheduler called\n";
    
    result += execute_iret(current_time);
    
    return result;
}

std::string simulate_cpu(int duration, int& current_time) {
    std::string result = std::to_string(current_time) + ", "
                       + std::to_string(duration) + ", CPU Burst\n";
    current_time += duration;
    return result;
}

std::string execute_isr(int device_num, int& current_time, std::vector<int>& delays,
                        const std::string& isr_type) {
    int isr_delay = delays[device_num];
    std::string result = std::to_string(current_time) + ", "
                       + std::to_string(isr_delay) + ", "
                       + isr_type + ": run the ISR\n";
    current_time += isr_delay;
    return result;
}

std::string execute_iret(int& current_time) {
    std::string result = std::to_string(current_time) + ", 1, IRET\n";
    current_time += 1;
    return result;
}

std::string switch_to_user_mode(int& current_time) {
    std::string result = std::to_string(current_time) + ", 1, switch to user mode\n";
    current_time += 1;
    return result;
}

std::string handle_interrupt(int device_num, int& current_time,
                            std::vector<std::string>& vectors,
                            std::vector<int>& delays,
                            const std::string& interrupt_type) {
    std::string result = "";
    const int CONTEXT_TIME = 10;
    
    auto [boilerplate, new_time] = intr_boilerplate(current_time, device_num, CONTEXT_TIME, vectors);
    result += boilerplate;
    current_time = new_time;
    result += execute_isr(device_num, current_time, delays, interrupt_type);
    result += execute_iret(current_time);
    
    return result;
}

std::pair<std::string, int> intr_boilerplate(int current_time, int intr_num, 
                                              int context_save_time, 
                                              std::vector<std::string> vectors) {
    std::string execution = "";
    
    execution += std::to_string(current_time) + ", 1, switch to kernel mode\n";
    current_time++;
    
    execution += std::to_string(current_time) + ", " + std::to_string(context_save_time) 
              + ", context saved\n";
    current_time += context_save_time;
    
    char vector_address_c[10];
    sprintf(vector_address_c, "0x%04X", (ADDR_BASE + (intr_num * VECTOR_SIZE)));
    std::string vector_address(vector_address_c);
    
    execution += std::to_string(current_time) + ", 1, find vector " + std::to_string(intr_num)
              + " in memory position " + vector_address + "\n";
    current_time++;
    
    std::string vector_addr = vectors.at(intr_num);
    vector_addr.erase(vector_addr.find_last_not_of(" \n\r\t") + 1);
    vector_addr = vector_addr.substr(vector_addr.find_first_not_of(" \n\r\t"));
    
    execution += std::to_string(current_time) + ", 1, load address " + vector_addr 
              + " into the PC\n";
    current_time++;
    
    return std::make_pair(execution, current_time);
}

std::vector<std::string> split_delim(std::string input, std::string delim) {
    std::vector<std::string> tokens;
    std::size_t pos = 0;
    std::string token;
    
    while ((pos = input.find(delim)) != std::string::npos) {
        token = input.substr(0, pos);
        tokens.push_back(token);
        input.erase(0, pos + delim.length());
    }
    tokens.push_back(input);
    return tokens;
}

std::tuple<std::string, int> parse_trace(std::string trace) {
    auto parts = split_delim(trace, ",");
    if (parts.size() < 2) {
        return {"null", -1};
    }
    
    auto activity = parts[0];
    int value = -1;
    
    try {
        value = std::stoi(parts[1]);
    } catch (...) {
        value = -1;
    }
    
    return {activity, value};
}

void write_output(std::string execution) {
    std::ofstream output_file("output_files/execution.txt");
    if (output_file.is_open()) {
        output_file << execution;
        output_file.close();
        std::cout << "File content generated successfully." << std::endl;
    }
}

void write_system_status_file(std::string status) {
    std::ofstream status_file("output_files/system_status.txt");
    if (status_file.is_open()) {
        status_file << status;
        status_file.close();
    }
}

int main(int argc, char** argv) {
    if(argc != 5) {
        std::cout << "ERROR! Expected 4 arguments\n";
        exit(1);
    }
    
    // Trace file is in input_files/ folder, everything else at root
    std::string trace_path = "input_files/" + std::string(argv[1]);
    std::string vector_path = std::string(argv[2]);  // Root
    std::string device_path = std::string(argv[3]);  // Root
    std::string external_path = std::string(argv[4]); // Root
    
    // Load vector table
    std::ifstream input_vector_table(vector_path);
    if (!input_vector_table.is_open()) {
        std::cerr << "Error: Unable to open " << vector_path << std::endl;
        exit(1);
    }
    
    std::vector<std::string> vectors;
    std::string vector;
    while(std::getline(input_vector_table, vector)) {
        vector.erase(vector.find_last_not_of(" \n\r\t") + 1);
        vector = vector.substr(vector.find_first_not_of(" \n\r\t"));
        vectors.push_back(vector);
    }
    input_vector_table.close();
    
    // Load device table (delays)
    std::vector<int> delays;
    std::ifstream device_table(device_path);
    if (!device_table.is_open()) {
        std::cerr << "Error: Unable to open " << device_path << std::endl;
        exit(1);
    }
    
    std::string duration;
    while(std::getline(device_table, duration)) {
        delays.push_back(std::stoi(duration));
    }
    device_table.close();
    
    // Open trace file
    std::ifstream input_file(trace_path);
    if (!input_file.is_open()) {
        std::cerr << "Error: Unable to open " << trace_path << std::endl;
        exit(1);
    }
    
    std::string trace;
    std::string execution;
    std::string status;
    int current_time = 0;
    int current_pid = 0;
    bool in_child_block = false;
    bool in_parent_block = false;
    
    initialize_system();
    external_files = load_external_files(external_path);
    
    for (size_t i = 0; i < std::numeric_limits<size_t>::max(); i++) {
        if (!std::getline(input_file, trace)) break;
        
        // Trim whitespace
        trace.erase(trace.find_last_not_of(" \n\r\t") + 1);
        trace = trace.substr(trace.find_first_not_of(" \n\r\t"));
        
        auto [activity, value] = parse_trace(trace);
        
        // === HANDLE BLOCK MARKERS ===
        if (activity == "IF_CHILD") {
            in_child_block = true;
            in_parent_block = false;
            continue;
        }
        if (activity == "IF_PARENT") {
            in_child_block = false;
            in_parent_block = true;
            
            // Transition: mark child as terminated and switch to parent
            for (auto& pcb : pcb_table) {
                if (pcb.pid == current_pid && current_pid != 0) {
                    pcb.state = "terminated";
                    break;
                }
            }
            current_pid = 0;
            
            for (auto& pcb : pcb_table) {
                if (pcb.pid == 0) {
                    pcb.state = "running";
                }
            }
            
            continue;
        }
        if (activity == "ENDIF") {
            in_child_block = false;
            in_parent_block = false;
            continue;
        }
        
        // === FORK ===
        if (activity == "FORK") {
            execution += handle_fork(current_time, vectors, current_pid);
            
            int child_pid = next_pid - 1;
            
            for (auto& pcb : pcb_table) {
                if (pcb.pid == 0) {
                    pcb.state = "running";
                }
                if (pcb.pid == child_pid) {
                    pcb.state = "waiting";
                }
            }
            
            status += "\ntime: " + std::to_string(current_time) + "; current trace: FORK, " + std::to_string(value) + "\n";
            status += "+-----+--------------+------------------+------+---------+\n";
            status += "| PID | program name | partition number | size | state   |\n";
            status += "+-----+--------------+------------------+------+---------+\n";
            
            for (int j = (int)pcb_table.size() - 1; j >= 0; j--) {
                const auto& pcb = pcb_table[j];
                status += "| " + std::to_string(pcb.pid) + " | " + pcb.program_name 
                    + " | " + std::to_string(pcb.partition_number) + " | " 
                    + std::to_string(pcb.size) + " | " + pcb.state + " |\n";
            }
            status += "+-----+--------------+------------------+------+---------+\n";
            
            current_pid = child_pid;
        }
        // === CPU ===
        else if (activity == "CPU") {
            execution += simulate_cpu(value, current_time);
        }
        // === EXEC ===
        else if (activity.size() >= 4 && activity.substr(0, 4) == "EXEC") {
            std::string program_name = activity.substr(5);
            program_name.erase(0, program_name.find_first_not_of(" \t\r\n"));
            program_name.erase(program_name.find_last_not_of(" \t\r\n") + 1);
            
            int exec_pid = (in_child_block) ? current_pid : 0;
            
            for (auto& pcb : pcb_table) {
                if (pcb.pid == exec_pid) {
                    pcb.state = "running";
                }
            }
            
            execution += handle_exec(program_name, value, current_time, vectors, external_files, exec_pid);
            
            status += "\ntime: " + std::to_string(current_time) + "; current trace: EXEC " + program_name + ", " + std::to_string(value) + "\n";
            status += "+-----+--------------+------------------+------+---------+\n";
            status += "| PID | program name | partition number | size | state   |\n";
            status += "+-----+--------------+------------------+------+---------+\n";
            
            for (int j = (int)pcb_table.size() - 1; j >= 0; j--) {
                const auto& pcb = pcb_table[j];
                status += "| " + std::to_string(pcb.pid) + " | " + pcb.program_name 
                    + " | " + std::to_string(pcb.partition_number) + " | " 
                    + std::to_string(pcb.size) + " | " + pcb.state + " |\n";
            }
            status += "+-----+--------------+------------------+------+---------+\n";
            
            // === LOAD AND EXECUTE PROGRAM FILE (at root) ===
            std::string program_file = program_name + ".txt";
            std::ifstream prog_file(program_file);
            
            if (prog_file.is_open()) {
                std::string program_instruction;
                while (std::getline(prog_file, program_instruction)) {
                    // Trim the instruction
                    program_instruction.erase(program_instruction.find_last_not_of(" \n\r\t") + 1);
                    program_instruction = program_instruction.substr(program_instruction.find_first_not_of(" \n\r\t"));
                    
                    if (program_instruction.empty()) continue;
                    
                    auto [prog_activity, prog_value] = parse_trace(program_instruction);
                    
                    if (prog_activity == "CPU") {
                        execution += simulate_cpu(prog_value, current_time);
                    }
                    else if (prog_activity == "SYSCALL" || prog_activity == "END_IO") {
                        execution += handle_interrupt(prog_value, current_time, vectors, delays, prog_activity);
                    }
                }
                prog_file.close();
            }
        }
        // === SYSCALL / INTERRUPT ===
        else if (activity == "SYSCALL" || activity == "END_IO") {
            execution += handle_interrupt(value, current_time, vectors, delays, activity);
        }
    }
    
    input_file.close();
    
    execution += "\nFinal System State\n";
    execution += "Partition Table:\n";
    for (const auto& part : partition_table) {
        execution += "Partition " + std::to_string(part.number) + ": "
                  + std::to_string(part.size) + " MB - Code: " + part.code + "\n";
    }
    
    execution += "\nPCB Table:\n";
    for (const auto& pcb : pcb_table) {
        execution += "PID " + std::to_string(pcb.pid);
        if (pcb.ppid != -1) {
            execution += " (Parent: " + std::to_string(pcb.ppid) + ")";
        }
        execution += ": " + pcb.program_name + " (Partition " + std::to_string(pcb.partition_number)
                  + ", " + std::to_string(pcb.size) + " MB, State: " + pcb.state + ")\n";
    }
    
    write_output(execution);
    write_system_status_file(status);
    
    return 0;
}
