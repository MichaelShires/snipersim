#include <CLI/CLI.hpp>
#include <curl/curl.h>
#include <iostream>
#include <fstream>
#include <limits.h>
#include <string>
#include <unistd.h>
#include <vector>

#include "config/config_resolver.h"
#include "sniper_exception.h"
#include "report.h"
#include "merge_llc.h"

extern int run_simulator(int argc, char* argv[]);

size_t write_data(void* ptr, size_t size, size_t nmemb, FILE* stream) {
   size_t written = fwrite(ptr, size, nmemb, stream);
   return written;
}

bool download_file(const std::string& url, const std::string& out_filename) {
   CURL* curl;
   FILE* fp;
   CURLcode res;
   curl = curl_easy_init();
   if (curl) {
      fp = fopen(out_filename.c_str(), "wb");
      if (!fp) {
         std::cerr << "Failed to open file for writing: " << out_filename << std::endl;
         return false;
      }
      curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
      curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
      std::cout << "[FETCH] Downloading " << url << "..." << std::endl;
      res = curl_easy_perform(curl);
      curl_easy_cleanup(curl);
      fclose(fp);
      if (res == CURLE_OK) {
         return true;
      } else {
         std::cerr << "Download failed: " << curl_easy_strerror(res) << std::endl;
         return false;
      }
   }
   return false;
}

#include <sys/wait.h>

std::string sniper_root;

int do_sim(const std::vector<std::string>& cmdline) {
   if (cmdline.empty()) {
      std::cerr << "[SIM] Error: no command line provided." << std::endl;
      return 1;
   }
   std::cout << "[SIM] Starting internal SDE orchestrator..." << std::endl;

   pid_t pid = fork();
   if (pid == 0) {
      // Child process: run SDE
      std::vector<const char*> sde_args;
      std::string sde_cmd = sniper_root + "/sde_kit/sde64";
      std::string recorder_path = "sde_sift_recorder.so"; // Must be just filename for sde64 logic

      sde_args.push_back(sde_cmd.c_str());
      sde_args.push_back("-t");
      sde_args.push_back(recorder_path.c_str());
      sde_args.push_back("-sniper:o");
      sde_args.push_back("trace");
      sde_args.push_back("--");
      for (const auto& arg : cmdline) {
         sde_args.push_back(arg.c_str());
      }
      sde_args.push_back(nullptr);

      // Add to LD_LIBRARY_PATH so sde can find the tool
      std::string current_ld = getenv("LD_LIBRARY_PATH") ? getenv("LD_LIBRARY_PATH") : "";
      std::string new_ld = sniper_root + "/lib:" + sniper_root + "/sde_kit/intel64:" + current_ld;
      setenv("LD_LIBRARY_PATH", new_ld.c_str(), 1);

      execvp(sde_cmd.c_str(), const_cast<char* const*>(sde_args.data()));
      std::cerr << "[SIM] Failed to exec " << sde_cmd << ". Ensure SDE is fetched." << std::endl;
      exit(1);
   } else if (pid > 0) {
      int status;
      waitpid(pid, &status, 0);
      if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
         std::cerr << "[SIM] SDE failed with status " << WEXITSTATUS(status) << std::endl;
         return 1;
      }
      std::cout << "[SIM] SDE finished, starting Sniper core..." << std::endl;

      std::vector<std::string> configs;
      config::ConfigResolver::resolve(sniper_root.empty() ? "config/base.cfg" : sniper_root + "/config/base.cfg",
                                      sniper_root, configs);
      config::ConfigResolver::resolve(sniper_root.empty() ? "config/gainestown.cfg" : sniper_root + "/config/gainestown.cfg",
                                      sniper_root, configs);

      // Run simulator
      std::vector<const char*> sim_args_c;
      sim_args_c.push_back("sniper");
      for (const auto& cfg : configs) {
         sim_args_c.push_back("-c");
         sim_args_c.push_back(cfg.c_str());
      }

      sim_args_c.push_back("--general/total_cores=1");
      sim_args_c.push_back("-g");
      sim_args_c.push_back("--general/output_dir=.");
      sim_args_c.push_back("-g");
      sim_args_c.push_back("--traceinput/trace_prefix=trace");
      sim_args_c.push_back("-g");
      sim_args_c.push_back("--traceinput/enabled=true");
      sim_args_c.push_back("-g");
      sim_args_c.push_back("--traceinput/emulate_syscalls=false");

      return run_simulator(sim_args_c.size(), const_cast<char**>(sim_args_c.data()));
   } else {
      std::cerr << "[SIM] Fork failed." << std::endl;
      return 1;
   }
}

int do_fetch(const std::string& dep_name) {
   if (dep_name == "pin") {
      std::string url =
          "https://software.intel.com/sites/landingpage/pintool/downloads/pin-3.28-98749-g6643ecee5-gcc-linux.tar.gz";
      std::string file = "pin.tar.gz";
      if (download_file(url, file)) {
         std::cout << "[FETCH] Successfully downloaded Pin." << std::endl;
         system("tar -xf pin.tar.gz");
      }
   } else if (dep_name == "sde") {
      std::string url = "https://downloadmirror.intel.com/813591/sde-external-9.33.0-2024-01-07-lin.tar.xz";
      std::string file = "sde.tar.xz";
      if (download_file(url, file)) {
         std::cout << "[FETCH] Successfully downloaded SDE." << std::endl;
         system("tar -xf sde.tar.xz");
      }
   } else {
      std::cerr << "Unknown dependency: " << dep_name << std::endl;
   }
   return 0;
}

// Function to handle self-injection for Relocatable Environment
void setup_relocatable_environment(char** argv) {
   char exe_path[PATH_MAX];
   ssize_t count = readlink("/proc/self/exe", exe_path, PATH_MAX);
   if (count != -1) {
      exe_path[count] = '\0';
      std::string path(exe_path);
      size_t last_slash = path.find_last_of('/');
      if (last_slash != std::string::npos) {
         std::string bin_dir = path.substr(0, last_slash);
         size_t second_last_slash = bin_dir.find_last_of('/');
         if (second_last_slash != std::string::npos) {
            sniper_root = bin_dir.substr(0, second_last_slash);
            std::string lib_dir = sniper_root + "/lib";

            const char* current_ld = getenv("LD_LIBRARY_PATH");
            bool needs_update = true;
            if (current_ld && std::string(current_ld).find(lib_dir) != std::string::npos) {
               needs_update = false;
            }

            if (needs_update) {
               std::string new_ld = lib_dir;
               if (current_ld) {
                  new_ld += ":" + std::string(current_ld);
               }
               setenv("LD_LIBRARY_PATH", new_ld.c_str(), 1);

               // Re-exec ourselves with the new environment
               execv(exe_path, argv);
            }
         }
      }
   }
}

int do_doctor() {
   std::cout << "=== Sniper Doctor (Native) ===" << std::endl;
   bool all_ok = true;

   auto check = [&](const std::string& name, bool condition, const std::string& remedy = "") {
      std::cout << "[" << (condition ? "OK" : "FAIL") << "] " << name;
      if (!condition) {
         std::cout << " -> Remedy: " << remedy;
         all_ok = false;
      }
      std::cout << std::endl;
   };

   // 1. Sniper Root
   check("Sniper Root", !sniper_root.empty(), "Ensure binary is run from build/bin/ or similar.");

   // 2. SDE Kit
   std::string sde_path = sniper_root + "/sde_kit/sde64";
   check("SDE Kit (sde64)", access(sde_path.c_str(), X_OK) == 0, "Run './sniper fetch sde'");

   // 3. SIFT Recorder
   std::string recorder_path = sniper_root + "/sde_sift_recorder.so";
   check("SIFT Recorder Tool", access(recorder_path.c_str(), R_OK) == 0,
         "Ensure 'sde_sift_recorder.so' is in Sniper root.");

   // 4. Config Files
   std::string base_cfg = sniper_root + "/config/base.cfg";
   check("Base Config (base.cfg)", access(base_cfg.c_str(), R_OK) == 0, "Check 'config/' directory integrity.");

   // 5. External Tools
   check("System 'sde64'", system("which sde64 > /dev/null 2>&1") == 0, "Add SDE to your PATH for easier recording.");
   check("System 'clang++'", system("clang++ --version > /dev/null 2>&1") == 0, "Install Clang 15+ for APX support.");

   // 6. Python Environment (for reporting tools)
   check("Python 3", system("python3 --version > /dev/null 2>&1") == 0, "Install python3 for statistics reporting.");

   // 7. Developer Libraries (Headers)
   check("libsqlite3-dev", system("pkg-config --exists sqlite3") == 0, "sudo apt install libsqlite3-dev");
   check("libcurl-dev", system("pkg-config --exists libcurl") == 0, "sudo apt install libcurl4-openssl-dev");
   check("zlib-dev", system("pkg-config --exists zlib") == 0, "sudo apt install zlib1g-dev");

   if (all_ok) {
      std::cout << "\n[PASS] Your environment is ready for High-Fidelity research." << std::endl;
      return 0;
   } else {
      std::cout << "\n[FAIL] Some dependencies are missing. Please follow the remedy steps." << std::endl;
      return 1;
   }
}

void handle_crash(const std::string& error_type, const std::string& message) {
   std::cerr << "\n[CRASH] Sniper encountered a fatal error!" << std::endl;
   std::cerr << "[CRASH] Error Type: " << error_type << std::endl;
   std::cerr << "[CRASH] Message: " << message << std::endl;

   std::string report_file = "sniper_crash_report.txt";
   std::ofstream report(report_file);
   if (report.is_open()) {
      report << "=== Sniper Crash Report ===" << std::endl;
      report << "Error Type: " << error_type << std::endl;
      report << "Message: " << message << std::endl;
      report << "\n--- System Information ---" << std::endl;
      report.flush();
      system("uname -a >> sniper_crash_report.txt 2>&1");
      system("uptime >> sniper_crash_report.txt 2>&1");
      report << "\n--- Sniper Environment ---" << std::endl;
      report << "Sniper Root: " << sniper_root << std::endl;
      report << "\n--- Loaded Shared Libraries ---" << std::endl;
      report.flush();
      system("ldd /proc/self/exe >> sniper_crash_report.txt 2>&1");
      report.close();
      std::cerr << "[CRASH] Debug information bundled into: " << report_file << std::endl;
   }
}

int main(int argc, char** argv) {
   try {
      setup_relocatable_environment(argv);

      CLI::App app{"Sniper Simulation Native CLI"};
      app.require_subcommand(1);

      // Subcommand: sim
      auto sim = app.add_subcommand("sim", "Full simulation: record and run in one step");
      sim->allow_extras();

      // Subcommand: run (just runs the simulator core)
      auto run = app.add_subcommand("run", "Run the Sniper simulator core");
      std::vector<std::string> run_configs;
      std::vector<std::string> run_overrides;
      run->add_option("-c,--config", run_configs, "Configuration files");
      run->add_option("-g,--override", run_overrides, "Global configuration overrides");
      run->allow_extras();

      // Subcommand: fetch
      auto fetch = app.add_subcommand("fetch", "Fetch dependencies using libcurl");
      std::string dep_name;
      fetch->add_option("dependency", dep_name, "Dependency to fetch (pin, sde, pinplay)")->required();

      // Subcommand: doctor
      auto doctor = app.add_subcommand("doctor", "Check environment and dependencies");

      // Subcommand: workloads
      auto workloads =
          app.add_subcommand("workloads", "Automated Science Pipelines: Fetch and compile workloads (PARSEC, SPEC)");
      std::string workload_name;
      workloads->add_option("name", workload_name, "Workload name (e.g., parsec, spec, coremark)")->required();

      // Subcommand: profile
      auto profile =
          app.add_subcommand("profile", "Profile the simulator hot-paths (InstructionDecoder, MicroOp dispatch)");
      std::string profiler_tool;
      profile->add_option("tool", profiler_tool, "Profiler to use (perf, gprof)")->default_val("perf");

      // Subcommand: lint
      auto lint = app.add_subcommand("lint", "Run static analysis and formatting checks");

      // Subcommand: bench
      auto bench = app.add_subcommand("bench", "Performance Regression Suite: Track simulation throughput (KIPS)");

      // Subcommand: package
      auto package = app.add_subcommand("package", "Deployment Packaging: Bundle verified environment into relocatable .tar.gz");

      // Subcommand: doc
      auto doc = app.add_subcommand("doc", "Integrated documentation access");
      std::string doc_topic;
      doc->add_option("topic", doc_topic, "Documentation topic (readme, developer, tutorial, gemini)");

      // Subcommand: report
      auto report = app.add_subcommand("report", "Native Reporting: Generate simulation summary (sim.out) natively");
      std::string results_dir = ".";
      std::string output_file = "sim.out";
      report->add_option("-d,--dir", results_dir, "Results directory (default: .)");
      report->add_option("-o,--output", output_file, "Output file (default: sim.out)");

      auto merge_llc = app.add_subcommand("merge-llc", "Merge LLC fusion edge CSVs with weights");
      std::vector<double> merge_weights;
      std::vector<std::string> merge_inputs;
      std::string merge_output = "final_weighted_edges.csv";
      merge_llc->add_option("-w,--weights", merge_weights, "Weights for each input file")->required();
      merge_llc->add_option("-i,--inputs", merge_inputs, "Input CSV files")->required();
      merge_llc->add_option("-o,--output", merge_output, "Output CSV file (default: final_weighted_edges.csv)");

      CLI11_PARSE(app, argc, argv);
      if (app.got_subcommand(merge_llc)) {
         run_merge_llc(merge_weights, merge_inputs, merge_output);
         return 0;
      } else if (app.got_subcommand(run)) {
         // Prepare args for run_simulator
         std::vector<char*> c_args;
         c_args.push_back(argv[0]);
         for (const auto& cfg : run_configs) {
            c_args.push_back(const_cast<char*>("-c"));
            c_args.push_back(const_cast<char*>(cfg.c_str()));
         }
         for (const auto& ovr : run_overrides) {
            c_args.push_back(const_cast<char*>("-g"));
            c_args.push_back(const_cast<char*>(ovr.c_str()));
         }
         std::vector<std::string> remaining = run->remaining();
         for (auto& arg : remaining) {
            c_args.push_back(const_cast<char*>(arg.c_str()));
         }
         return run_simulator(c_args.size(), c_args.data());
      } else if (app.got_subcommand(sim)) {
         return do_sim(sim->remaining());
      } else if (app.got_subcommand(fetch)) {
         return do_fetch(dep_name);
      } else if (app.got_subcommand(doctor)) {
         return do_doctor();
      } else if (app.got_subcommand(workloads)) {
         std::cout << "[WORKLOADS] Automated Science Pipeline." << std::endl;
         std::cout << "[WORKLOADS] Initializing " << workload_name << " fetch and compile..." << std::endl;
         if (workload_name == "coremark") {
            std::cout << "[WORKLOADS] Fetching CoreMark from GitHub..." << std::endl;
            std::string url = "https://github.com/eembc/coremark/archive/refs/heads/master.tar.gz";
            std::string file = "coremark.tar.gz";
            if (download_file(url, file)) {
               system("mkdir -p workloads/coremark");
               system("tar -xf coremark.tar.gz -C workloads/coremark --strip-components=1");
               std::cout << "[WORKLOADS] Building CoreMark..." << std::endl;
               system("cd workloads/coremark && make compile");
               std::cout << "[WORKLOADS] CoreMark is ready at workloads/coremark/coremark.exe" << std::endl;
            }
         } else {
            std::cout << "[WORKLOADS] Requires PARSEC/SPEC license or access. (Stubbed natively)" << std::endl;
            std::cout << "[WORKLOADS] Automated fetch only available for open-source CoreMark." << std::endl;
         }
         return 0;
      } else if (app.got_subcommand(profile)) {
         std::cout << "[PROFILE] Simulator Performance Profiling & Optimization." << std::endl;
         std::cout << "[PROFILE] To profile the hot-paths, running: " << profiler_tool << " record ./build/sniper sim ..."
                   << std::endl;
         std::cout << "[PROFILE] (Profiling integration stubbed for Native CLI)" << std::endl;
         return 0;
      } else if (app.got_subcommand(lint)) {
         std::cout << "=== Sniper Lint (Static Analysis & Formatting) ===" << std::endl;
         std::cout << "[LINT] Checking code style with clang-format..." << std::endl;
         std::string format_cmd = "find " + sniper_root + "/common " + sniper_root + "/sift " + sniper_root +
                                  "/standalone -name '*.cc' -o -name '*.h' | xargs -P $(nproc) clang-format -n --Werror";
         int format_ret = system(format_cmd.c_str());
         if (format_ret == 0) {
            std::cout << "[LINT] Code style check passed!" << std::endl;
         } else {
            std::cout << "[LINT] Code style check failed." << std::endl;
         }

         std::cout << "[LINT] Running static analysis with clang-tidy..." << std::endl;
         std::string tidy_cmd = "clang-tidy " + sniper_root + "/standalone/cli.cc -- -I" + sniper_root + "/build_asan/_deps/cli11-src/include -I" + sniper_root + "/common/misc -I" + sniper_root + "/standalone";
         int tidy_ret = system(tidy_cmd.c_str());
         if (tidy_ret == 0) {
            std::cout << "[LINT] Static analysis passed!" << std::endl;
         } else {
            std::cout << "[LINT] Static analysis found issues." << std::endl;
         }

         return (format_ret == 0 && tidy_ret == 0) ? 0 : 1;
      } else if (app.got_subcommand(bench)) {
         std::cout << "=== Sniper Performance Benchmarking ===" << std::endl;
         std::cout << "[BENCH] Running matrix multiply baseline (matmul)..." << std::endl;

         // Ensure matmul is compiled
         std::string compile_cmd =
             "gcc -O2 " + sniper_root + "/samples/matmul.c -o " + sniper_root + "/samples/matmul > /dev/null 2>&1";
         system(compile_cmd.c_str());

         // Run simulation and capture output
         std::string sim_cmd = sniper_root + "/build/sniper sim -- " + sniper_root + "/samples/matmul 2>&1";
         FILE* pipe = popen(sim_cmd.c_str(), "r");
         if (!pipe) return 1;

         char buffer[128];
         std::string result = "";
         while (!feof(pipe)) {
            if (fgets(buffer, 128, pipe) != NULL) result += buffer;
         }
         pclose(pipe);

         // Extract KIPS
         size_t pos = result.find("Simulation speed");
         if (pos != std::string::npos) {
            std::string speed_line = result.substr(pos, result.find("\n", pos) - pos);
            std::cout << "[BENCH] " << speed_line << std::endl;

            // Extract numeric value
            double actual_kips = 0.0;
            sscanf(speed_line.c_str(), "Simulation speed %lf", &actual_kips);

            // Load baseline
            double baseline_kips = 0.0;
            std::ifstream baseline_file(sniper_root + "/config/baseline.perf");
            std::string line;
            while (std::getline(baseline_file, line)) {
               if (line.find("matmul:") == 0) {
                  baseline_kips = std::stod(line.substr(7));
                  break;
               }
            }

            if (baseline_kips > 0) {
               std::cout << "[BENCH] Baseline: " << baseline_kips << " KIPS" << std::endl;
               if (actual_kips < baseline_kips * 0.8) {
                  std::cout << "[BENCH] WARNING: Performance regression detected! (>20% slowdown)" << std::endl;
               } else {
                  std::cout << "[BENCH] Performance verified against Golden Baseline." << std::endl;
               }
            } else {
               std::cout << "[BENCH] Warning: No baseline found for matmul." << std::endl;
            }
         } else {
            std::cout << "[BENCH] Error: Could not extract simulation speed." << std::endl;
            return 1;
         }
         return 0;
      }
      else if (app.got_subcommand(package)) {
         std::cout << "=== Sniper Deployment Packaging ===" << std::endl;
         std::string tar_cmd = "tar -czf sniper_release.tar.gz -C " + sniper_root +
                               " build/bin build/lib config scripts sde_kit sde_sift_recorder.so samples README.md "
                               "DEVELOPER.md";
         std::cout << "[PACKAGE] Creating sniper_release.tar.gz..." << std::endl;
         int ret = system(tar_cmd.c_str());
         if (ret == 0) {
            std::cout << "[PACKAGE] Release bundle created successfully." << std::endl;
         }
         else {
            std::cout << "[PACKAGE] Failed to create release bundle." << std::endl;
         }
         return ret;
      }
      else if (app.got_subcommand(doc)) {
         if (doc_topic.empty()) {
            std::cout << "Available documentation topics:" << std::endl;
            std::cout << "  readme    - Main Sniper README" << std::endl;
            std::cout << "  developer - Developer Guide and Architecture Map" << std::endl;
            std::cout << "  tutorial  - End-to-end tutorial" << std::endl;
            std::cout << "  gemini    - Modernization roadmap and task status" << std::endl;
            return 0;
         }
         std::string filename;
         if (doc_topic == "readme") filename = "README.md";
         else if (doc_topic == "developer") filename = "DEVELOPER.md";
         else if (doc_topic == "tutorial") filename = "TUTORIAL.md";
         else if (doc_topic == "gemini") filename = "GEMINI.md";
         else {
            std::cerr << "Unknown topic: " << doc_topic << std::endl;
            return 1;
         }
         std::string cmd = "cat " + sniper_root + "/" + filename + " | ${PAGER:-less}";
         return system(cmd.c_str());
      }
      else if (app.got_subcommand(report)) {
         std::cout << "=== Sniper Native Reporting ===" << std::endl;
         std::string db_path = results_dir + "/sim.stats.sqlite3";
         ReportGenerator gen(db_path);
         if (gen.generate(output_file)) {
            return 0;
         }
         else {
            std::cerr << "[REPORT] Failed to generate report from " << db_path << std::endl;
            return 1;
         }
      }

      return 0;
   } catch (const SniperException& e) {
      handle_crash("SniperException", e.what());
      return 1;
   } catch (const std::exception& e) {
      handle_crash("StandardException", e.what());
      return 1;
   } catch (...) {
      handle_crash("UnknownException", "No message available");
      return 1;
   }
}
