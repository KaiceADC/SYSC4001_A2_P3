#ifndef INTERRUPTS_HPP_
#define INTERRUPTS_HPP_

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <tuple>
#include <cstdio>
#include <queue>
#include <map>
#include <limits>


// Simulator constants
#define ADDR_BASE 0        // Base address for vector table
#define VECTOR_SIZE 2      // Each vector entry is 2 bytes
#define CPU_SPEED 100      // CPU clock speed (for future use)
#define MEM_LIMIT 1        // Memory limit per process (for future use)

// Memory partition - represents a fixed memory region allocated to processes
struct Partition {
    unsigned int number;    // Partition ID (0-4, 5 reserved for init)
    unsigned int size;      // Size in megabytes
    std::string code;       // Current contents: "free", "init", or program name
};

// Process Control Block - stores all information about a running process
struct PCB {
    int pid;                    // Process identifier (unique)
    int ppid;                   // Parent process ID (-1 if init)
    std::string program_name;   // Name of program currently executing
    int partition_number;       // Allocated memory partition
    int size;                   // Program size in MB
    std::string state;          // Process state: "running", "waiting", "ready", "terminated"
    int priority;               // Priority: 0=normal, 1=child (child executes first)
};

// External file structure - represents a program stored on simulated disk
struct ExternalFile {
    std::string program_name;   // Program name
    unsigned int size;          // Size in MB
};

// System initialization - sets up memory partitions and init process
void initialize_system();

// Load programs from file - simulates disk storage for EXEC operations
std::vector<ExternalFile> load_external_files(const std::string& filename);

// FORK system call - creates child process by cloning parent PCB
std::string handle_fork(int& current_time, std::vector<std::string>& vectors, int current_pid);

// EXEC system call - loads program from disk into memory partition
std::string handle_exec(const std::string& program_name, int& current_time,
                        std::vector<std::string>& vectors, 
                        std::vector<ExternalFile>& external_files,
                        int current_pid);

// Memory management - find free partition using first-fit allocation
// Returns partition number on success, -1 if no suitable partition
int find_available_partition(unsigned int program_size);

// Process scheduling - get next ready process in queue
// Returns process ID, or -1 if queue is empty
int get_next_process();

// Add process to ready queue for execution
void add_to_ready_queue(int pid);

// Remove process from ready queue (cleanup after termination)
void remove_from_ready_queue(int pid);

// Check if process is child of another process
bool is_child_of(int pid, int parent_pid);

// Mark process as terminated for cleanup
void terminate_process(int pid);

// CPU simulation - execute for specified duration in milliseconds
std::string simulate_cpu(int duration, int& current_time);

// Execute interrupt service routine for a device
// Duration from delays.txt, indexed by device_num
std::string execute_isr(int device_num, int& current_time, std::vector<int>& delays,
                        const std::string& isr_type);

// Return from interrupt instruction (1ms operation)
std::string execute_iret(int& current_time);

// Restore processor context from stack after interrupt (10ms standard)
std::string restore_context(int& current_time);

// Switch from kernel mode to user mode (1ms operation)
std::string switch_to_user_mode(int& current_time);

// Complete interrupt handling sequence
// Combines: entry boilerplate + ISR execution + exit sequence
std::string handle_interrupt(int device_num, int& current_time,
                            std::vector<std::string>& vectors,
                            std::vector<int>& delays,
                            const std::string& interrupt_type);

// Standard interrupt entry sequence for all interrupts
// Performs: kernel mode switch -> context save -> vector lookup -> PC load
// Returns: (execution trace string, updated time)
std::pair<std::string, int> intr_boilerplate(int current_time, int intr_num, 
                                              int context_save_time, 
                                              std::vector<std::string> vectors);

// Parse command line arguments and load all input files
// Arguments: ./program trace_file vectors_file delays_file external_files_file
// Returns: (vector_table, delay_table)
std::tuple<std::vector<std::string>, std::vector<int>> parse_args(int argc, char** argv);

// Parse single trace line - extract activity name and numeric value
// Example: "FORK,10" -> ("FORK", 10)
std::tuple<std::string, int> parse_trace(std::string trace);

// String utility - split string by delimiter
// Example: split_delim("a,b,c", ",") -> ["a", "b", "c"]
std::vector<std::string> split_delim(std::string input, std::string delim);

// Write execution trace to file (execution.txt)
// Contains all simulation events with timestamps and final system state
void write_output(std::string execution);

// Write system status to file (system_status.txt)
// Records PCB snapshots after major operations
void write_system_status_file(std::string status);

// Global variables - system state maintained throughout execution
extern std::vector<Partition> partition_table;              // Memory partitions
extern std::vector<PCB> pcb_table;                          // All processes
extern std::vector<ExternalFile> external_files;           // Available programs
extern std::queue<int> ready_queue;                         // Ready processes for scheduling
extern std::map<int, std::vector<int>> parent_child_map;   // Parent-child relationships

#endif
