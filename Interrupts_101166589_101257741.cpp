/**
 * @file interrupts.cpp
 * Implementation of OS Simulator - Part 3 and Part 4
 * Handles CPU execution, interrupts, process management, and system calls
 */

#include "Interrupts_101166589_101257741.hpp"

// Global state variables
std::vector<Partition> partition_table;
std::vector<PCB> pcb_table;
std::vector<ExternalFile> external_files;
std::queue<int> ready_queue;
std::map<int, std::vector<int>> parent_child_map;

int next_pid = 1;
int suspended_parent_pid = -1;

// Initialize system with memory partitions and init process
void initialize_system() {
    // Create 5 fixed memory partitions with different sizes
    partition_table = {
        {0, 40, "free"},
        {1, 25, "free"},
        {2, 15, "free"},
        {3, 10, "free"},
        {4, 8, "free"}
    };
    
    // Create init process - PID 0, runs at startup
    PCB init_process;
    init_process.pid = 0;
    init_process.ppid = -1;
    init_process.program_name = "init";
    init_process.partition_number = 5;  // Reserved partition
    init_process.size = 0;
    init_process.state = "running";
    init_process.priority = 0;
    
    pcb_table.push_back(init_process);
    ready_queue.push(0);
    next_pid = 1;
}

// Load available programs from external file (simulates disk storage)
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

// Find free partition for program using first-fit allocation
// Returns partition number if found, -1 if no suitable partition
int find_available_partition(unsigned int program_size) {
    for (auto& part : partition_table) {
        if (part.code == "free" && part.size >= program_size) {
            return part.number;
        }
    }
    return -1;
}

// Add process to ready queue for scheduling
void add_to_ready_queue(int pid) {
    ready_queue.push(pid);
}

// Get next process from ready queue in FIFO order
// Returns -1 if queue is empty
int get_next_process() {
    if (ready_queue.empty()) return -1;
    int next = ready_queue.front();
    ready_queue.pop();
    return next;
}

// Remove specific process from ready queue
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

// Check if pid is a child of parent_pid
bool is_child_of(int pid, int parent_pid) {
    for (auto& pcb : pcb_table) {
        if (pcb.pid == pid && pcb.ppid == parent_pid) {
            return true;
        }
    }
    return false;
}

// Mark process as terminated for cleanup
void terminate_process(int pid) {
    for (auto& pcb : pcb_table) {
        if (pcb.pid == pid) {
            pcb.state = "terminated";
            break;
        }
    }
}

// Handle FORK system call - creates child process by cloning parent
std::string handle_fork(int& current_time, std::vector<std::string>& vectors, int current_pid) {
    std::string result = "";
    const int CONTEXT_TIME = 10;
    
    // Interrupt entry: switch to kernel mode, save context, lookup vector
    auto [boilerplate, new_time] = intr_boilerplate(current_time, 2, CONTEXT_TIME, vectors);
    result += boilerplate;
    current_time = new_time;
    
    // Find parent process in PCB table
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
        result += restore_context(current_time);
        result += switch_to_user_mode(current_time);
        return result;
    }
    
    // Clone parent process to create child
    PCB child = parent;
    child.pid = next_pid++;           // Assign new unique PID
    child.ppid = current_pid;         // Set parent relationship
    child.priority = 1;               // Child has higher priority
    pcb_table.push_back(child);
    parent_child_map[current_pid].push_back(child.pid);
    add_to_ready_queue(child.pid);
    
    // Log PCB cloning operation
    result += std::to_string(current_time) + ", 1, cloning the PCB\n";
    current_time += 1;
    
    // Call scheduler (no time cost)
    result += std::to_string(current_time) + ", 0, scheduler called\n";
    
    // Return from interrupt
    result += execute_iret(current_time);
    result += restore_context(current_time);
    result += switch_to_user_mode(current_time);
    
    return result;
}

// Handle EXEC system call - load program from disk into memory partition
std::string handle_exec(const std::string& program_name, int& current_time,
                        std::vector<std::string>& vectors, 
                        std::vector<ExternalFile>& external_files,
                        int current_pid) {
    std::string result = "";
    const int CONTEXT_TIME = 10;
    
    // Interrupt entry sequence
    auto [boilerplate, new_time] = intr_boilerplate(current_time, 3, CONTEXT_TIME, vectors);
    result += boilerplate;
    current_time = new_time;
    
    // Search for requested program in external files (disk)
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
        result += restore_context(current_time);
        result += switch_to_user_mode(current_time);
        return result;
    }
    
    // Use first-fit to find available partition
    int partition_to_use = find_available_partition(program_size);
    
    if (partition_to_use == -1) {
        result += std::to_string(current_time) + ", 1, ERROR: No partition\n";
        current_time += 1;
        result += execute_iret(current_time);
        result += restore_context(current_time);
        result += switch_to_user_mode(current_time);
        return result;
    }
    
    // Mark partition as occupied with program
    for (auto& part : partition_table) {
        if (part.number == partition_to_use) {
            part.code = program_name;
            break;
        }
    }
    
    // Simulate disk load operation: 15ms per MB
    int loader_time = program_size * 15;
    result += std::to_string(current_time) + ", " + std::to_string(loader_time)
            + ", loading " + program_name + " from disk to partition " 
            + std::to_string(partition_to_use) + "\n";
    current_time += loader_time;
    
    result += std::to_string(current_time) + ", 1, marking partition as occupied\n";
    current_time += 1;
    
    // Update process control block with new program info
    result += std::to_string(current_time) + ", 3, updating PCB\n";
    current_time += 3;
    
    for (auto& pcb : pcb_table) {
        if (pcb.pid == current_pid) {
            pcb.program_name = program_name;
            pcb.partition_number = partition_to_use;
            pcb.size = program_size;
            break;
        }
    }
    
    // Scheduler processes context switch
    result += std::to_string(current_time) + ", 0, scheduler called\n";
    
    // Return from interrupt
    result += execute_iret(current_time);
    result += restore_context(current_time);
    result += switch_to_user_mode(current_time);
    
    return result;
}

// Simulate CPU execution for specified duration in milliseconds
std::string simulate_cpu(int duration, int& current_time) {
    std::string result = std::to_string(current_time) + ", "
                       + std::to_string(duration) + ", CPU execution\n";
    current_time += duration;
    return result;
}

// Execute interrupt service routine for specific device
// ISR duration determined by delays.txt indexed by device_num
std::string execute_isr(int device_num, int& current_time, std::vector<int>& delays,
                        const std::string& isr_type) {
    int isr_delay = delays[device_num];
    std::string result = std::to_string(current_time) + ", "
                       + std::to_string(isr_delay) + ", "
                       + isr_type + ": run the ISR\n";
    current_time += isr_delay;
    return result;
}

// Return from interrupt instruction (1ms)
std::string execute_iret(int& current_time) {
    std::string result = std::to_string(current_time) + ", 1, IRET\n";
    current_time += 1;
    return result;
}

// Restore processor context from stack (10ms standard time)
std::string restore_context(int& current_time) {
    const int CONTEXT_TIME = 10;
    std::string result = std::to_string(current_time) + ", "
                       + std::to_string(CONTEXT_TIME) + ", context restored\n";
    current_time += CONTEXT_TIME;
    return result;
}

// Switch from kernel mode to user mode (1ms)
std::string switch_to_user_mode(int& current_time) {
    std::string result = std::to_string(current_time) + ", 1, switch to user mode\n";
    current_time += 1;
    return result;
}

// Complete interrupt handling: entry -> ISR -> exit
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
    result += restore_context(current_time);
    result += switch_to_user_mode(current_time);
    
    return result;
}

// Standard interrupt entry sequence used for all interrupts
// Steps: kernel mode switch -> context save -> vector lookup -> PC load
std::pair<std::string, int> intr_boilerplate(int current_time, int intr_num, 
                                              int context_save_time, 
                                              std::vector<std::string> vectors) {
    std::string execution = "";
    
    // Switch to kernel mode
    execution += std::to_string(current_time) + ", 1, switch to kernel mode\n";
    current_time++;
    
    // Save CPU context to stack
    execution += std::to_string(current_time) + ", " + std::to_string(context_save_time) 
              + ", context saved\n";
    current_time += context_save_time;
    
    // Calculate vector table address
    char vector_address_c[10];
    sprintf(vector_address_c, "0x%04X", (ADDR_BASE + (intr_num * VECTOR_SIZE)));
    std::string vector_address(vector_address_c);
    
    // Find vector entry in memory
    execution += std::to_string(current_time) + ", 1, find vector " + std::to_string(intr_num)
              + " in memory position " + vector_address + "\n";
    current_time++;
    
    // Load ISR address into program counter
    execution += std::to_string(current_time) + ", 1, load address " + vectors.at(intr_num) 
              + " into the PC\n";
    current_time++;
    
    return std::make_pair(execution, current_time);
}

// Split string by delimiter - utility for parsing
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

// Parse trace file line into activity name and numeric value
// Example: "FORK,10" -> ("FORK", 10)
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

// Parse command line arguments and load all required input files
// Arguments: trace_file, vector_table, device_delays, external_programs
std::tuple<std::vector<std::string>, std::vector<int>> parse_args(int argc, char** argv) {
    if(argc != 5) {
        std::cout << "ERROR! Expected 4 arguments\n";
        std::cout << "Usage: ./interrupts <trace> <vectors> <delays> <external_files>\n";
        exit(1);
    }
    
    // Verify trace file exists
    std::ifstream input_file(argv[1]);
    if (!input_file.is_open()) {
        std::cerr << "Error: Unable to open " << argv[1] << std::endl;
        exit(1);
    }
    input_file.close();
    
    // Load interrupt vector addresses
    std::ifstream input_vector_table(argv[2]);
    if (!input_vector_table.is_open()) {
        std::cerr << "Error: Unable to open " << argv[2] << std::endl;
        exit(1);
    }
    
    std::string vector;
    std::vector<std::string> vectors;
    while(std::getline(input_vector_table, vector)) {
        vectors.push_back(vector);
    }
    input_vector_table.close();
    
    // Load device delay table
    std::string duration;
    std::vector<int> delays;
    std::ifstream device_table(argv[3]);
    if (!device_table.is_open()) {
        std::cerr << "Error: Unable to open " << argv[3] << std::endl;
        exit(1);
    }
    
    while(std::getline(device_table, duration)) {
        delays.push_back(std::stoi(duration));
    }
    device_table.close();
    
    return {vectors, delays};
}

// Write execution trace to output file
void write_output(std::string execution) {
    std::ofstream output_file("output_files/execution.txt");  // ✅ FIXED
    if (output_file.is_open()) {
        output_file << execution;
        output_file.close();
        std::cout << "File content generated successfully." << std::endl;
    } else {
        std::cerr << "Error opening output_files/execution.txt!" << std::endl;
    }
}


void write_system_status_file(std::string status) {
    std::ofstream status_file("output_files/system_status.txt");  // ✅ FIXED
    if (status_file.is_open()) {
        status_file << status;
        status_file.close();
    }
}

int main(int argc, char** argv) {
    auto [vectors, delays] = parse_args(argc, argv);
    std::ifstream input_file(argv[1]);
    std::string trace;
    std::string execution;
    std::string status;  // This will collect all status snapshots
    int current_time = 0;
    int current_pid = 0;
    bool in_child_block = false;
    bool in_parent_block = false;
    int block_pid = -1;
    
    initialize_system();
    external_files = load_external_files(argv[4]);
    
    // Process each trace line
    while(std::getline(input_file, trace)) {
        auto [activity, value] = parse_trace(trace);
        
        if(activity == "CPU") {
            execution += simulate_cpu(value, current_time);
        }
        else if (activity == "FORK") {
            execution += handle_fork(current_time, vectors, current_pid);
            
            // After FORK, update states: parent waits, child runs
            for (auto& pcb : pcb_table) {
                if (pcb.pid == current_pid) {  // Parent
                    pcb.state = "waiting";
                }
                if (pcb.pid == next_pid - 1) {  // Child (just created)
                    pcb.state = "running";
                }
            }
            
            // ADD STATUS SNAPSHOT AFTER FORK
            status += "\ntime: " + std::to_string(current_time) + "; current trace: FORK, " + std::to_string(value) + " //clones init, and runs the child\n";
            status += "+---+---+---+---+\n";
            status += "| PID | program name | partition number | size | state |\n";
            status += "+---+---+---+---+\n";
            for (const auto& pcb : pcb_table) {
                status += "| " + std::to_string(pcb.pid) + " | " + pcb.program_name 
                    + " | " + std::to_string(pcb.partition_number) + " | " 
                    + std::to_string(pcb.size) + " | " + pcb.state + " |\n";
            }
            status += "+---+---+---+---+\n";
        }

        else if (activity == "IF_CHILD") {
            in_child_block = true;
            in_parent_block = false;
            block_pid = value;
        }
        else if (activity == "IF_PARENT") {
            in_child_block = false;
            in_parent_block = true;
            block_pid = value;
        }
        else if (activity == "ENDIF") {
            in_child_block = false;
            in_parent_block = false;
            block_pid = -1;
        }
        else if (activity.substr(0, 4) == "EXEC") {
            std::string program_name = activity.substr(5);
            execution += handle_exec(program_name, current_time, vectors, external_files, current_pid);
            
            // ADD STATUS SNAPSHOT AFTER EXEC - only show affected process
            status += "\ntime: " + std::to_string(current_time) + "; current trace: EXEC " + program_name + ", " + std::to_string(value) + "\n";
            status += "+---+---+---+---+\n";
            status += "| PID | program name | partition number | size | state |\n";
            status += "+---+---+---+---+\n";
            
            // Show only processes that matter
            for (const auto& pcb : pcb_table) {
                if (pcb.pid == current_pid || pcb.pid == 0) {  // Show current and init
                    status += "| " + std::to_string(pcb.pid) + " | " + pcb.program_name 
                        + " | " + std::to_string(pcb.partition_number) + " | " 
                        + std::to_string(pcb.size) + " | " + pcb.state + " |\n";
                }
            }
            status += "+---+---+---+---+\n";
        }

        else if (activity == "SYSCALL" || activity == "END_IO"){
            execution += handle_interrupt(value, current_time, vectors, delays, activity);
        }
    }
    
    input_file.close();
    
    // Append final system state
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
    write_system_status_file(status);  // Now status has data!
    
    return 0;
}
