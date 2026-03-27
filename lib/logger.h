#ifndef LOGGER_H
#define LOGGER_H

// increase limit if needed
#define LOG_FILENAME_MAX_LENGTH 64
#define LOG_ENTRY_MAX_LENGTH 256  

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

enum LogLevel {
  DEBUG,
  INFO
};

struct Logger {
  bool generateLogFile;
  char log_filename[LOG_FILENAME_MAX_LENGTH];
  int verbosity;
};

void generate_log(struct Logger logger, const char message[], int verbosity) {
  // Generate & print a log entry with a timestamp & message. 
  // Optionally write the log entry to a file if logger is configured to do so.
  if (verbosity < logger.verbosity) return; 
  
  time_t now;
  char time_buf[32];
  time(&now);
  struct tm* time_info = localtime(&now);
  strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", time_info);
  char log_entry[LOG_ENTRY_MAX_LENGTH];
  snprintf(log_entry, sizeof(log_entry), "[%s] \t%s\n", time_buf, message);
  printf("%s", log_entry);

  if (!logger.generateLogFile) return;
  // Write the log entry to a file
  write_log_file(logger, log_entry);
}

void log_info(struct Logger logger, const char message[]) {
  generate_log(logger, message, INFO);
}

void log_debug(struct Logger logger, const char message[]) {
  generate_log(logger, message, DEBUG);
}

struct Logger create_logger(int verbosity, 
                            bool generateLogFile, 
                            const char* log_filename) {
/*
 * Creates a Logger with the configured verbosity and file-output behavior.
 * @param verbosity Minimum log level to emit (messages with level >= verbosity are logged).
 * @param generateLogFile If true, log entries are also appended to a file.
 * @param log_filename Filename to use when generateLogFile is true; if NULL, a default timestamp-based name is used.
 */
  struct Logger logger;
  logger.generateLogFile = generateLogFile;
  if (generateLogFile) {
    if (log_filename == NULL) {
      // Set to default log filename with timestamp
      time_t now;
      time(&now);
      struct tm* time_info = localtime(&now);
      strftime(logger.log_filename, sizeof(logger.log_filename), "log_%Y-%m-%d %H%M%S.log", time_info);
    } else {
      snprintf(logger.log_filename, sizeof(logger.log_filename), "%s", log_filename);
    }
  }
  logger.verbosity = verbosity;
  return logger;
}

void write_log_file(struct Logger logger, const char log_entry[]) {
  // Writes log entry to a file.
  if (!logger.generateLogFile) return; // Double check if log file generation is enabled

  // The slow way: Open the file, write the log entry, and close the file for each message.
  FILE* log_file = fopen(logger.log_filename, "a");
  if (log_file == NULL) {
    fprintf(stderr, "Error: Log file is not available.\n");
    return;
  }
  fprintf(log_file, "%s", log_entry);
  if (ferror(log_file)) {
    fprintf(stderr, "Error: Unable to write to log file.\n");
    fclose(log_file);
    return;
  }
  fflush(log_file); // Ensure the log entry is written to the file immediately
  fclose(log_file);
}


#endif
