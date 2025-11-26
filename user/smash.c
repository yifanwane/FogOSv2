#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

#define MAX_HISTORY 100
#define MAX_CMD_LEN 128
#define MAX_ARGS 16

// --- History Entry Structure ---
struct history_entry {
    char cmd[MAX_CMD_LEN];
    int duration_ms; 
};

struct history_entry history[MAX_HISTORY];

// --- Tokenizer Helpers ---
uint strspn(const char *str, const char *chars) {
  uint i, j;
  for (i = 0; str[i] != '\0'; i++) {
    for (j = 0; chars[j] != str[i]; j++) {
      if (chars[j] == '\0') return i;
    }
  }
  return i;
}

uint strcspn(const char *str, const char *chars) {
  const char *p, *sp;
  char c, sc;
  for (p = str;;) {
    c = *p++;
    sp = chars;
    do {
      if ((sc = *sp++) == c) return (p - 1 - str);
    } while (sc != 0);
  }
}

char *next_token(char **str_ptr, const char *delim) {
  if (*str_ptr == 0) return 0;
  uint tok_start = strspn(*str_ptr, delim);
  uint tok_end = strcspn(*str_ptr + tok_start, delim);
  if (tok_end == 0) { *str_ptr = 0; return 0; }
  char *current_ptr = *str_ptr + tok_start;
  *str_ptr += tok_start + tok_end;
  if (**str_ptr == '\0') *str_ptr = 0;
  else { **str_ptr = '\0'; (*str_ptr)++; }
  return current_ptr;
}

// --- Path Execution Logic (execvp) ---
// Priority:
// 1. Absolute/Relative path (contains '/') -> Run directly
// 2. Root directory (e.g., /ls)
// 3. Current directory (e.g., ls)
void
execvp(char *cmd, char **args)
{
  // 1. Check if it contains a slash (Absolute or relative path)
  if(strchr(cmd, '/') != 0){
    exec(cmd, args);
    // If we are here, exec failed
    fprintf(2, "exec: %s failed\n", cmd);
    exit(1);
  }

  // 2. Try Root Directory First (e.g. /ls)
  // Construct path: "/" + cmd
  char buf[128];
  buf[0] = '/';
  char *p = buf + 1;
  char *q = cmd;
  // Safe copy
  while(*q && (p - buf < 127)){
      *p++ = *q++;
  }
  *p = 0; // Null terminate
  
  exec(buf, args);
  // If we are here, finding it in / failed.

  // 3. Try Current Directory
  exec(cmd, args);
  
  // 4. Final Failure (Found nowhere)
  fprintf(2, "exec: %s failed\n", cmd);
  exit(1);
}

// --- Main Shell Logic ---

int main(int argc, char *argv[]) {
  char buf[MAX_CMD_LEN];
  char cwd_buf[128]; 
  
  int cmd_count = 1;      
  int last_status = 0;
  int is_script_mode = 0;

  if (argc > 1) {
    close(0);
    if (open(argv[1], O_RDONLY) < 0) {
      fprintf(2, "smash: cannot open %s\n", argv[1]);
      exit(1);
    }
    is_script_mode = 1;
  }

  while (1) {
    if (!is_script_mode) {
      if (getcwd(cwd_buf, sizeof(cwd_buf)) != 0) strcpy(cwd_buf, "error");
      printf("[%d]-[%d]─[%s]$ ", last_status, cmd_count, cwd_buf);
    }

    memset(buf, 0, sizeof(buf));
    char *input = gets(buf, sizeof(buf));
    if (input == 0) break; 
    if(strlen(buf) > 0 && buf[strlen(buf)-1] == '\n') buf[strlen(buf)-1] = 0;

    // Handle Comments
    for(int i=0; i<strlen(buf); i++) {
        if(buf[i] == '#') { buf[i] = '\0'; break; }
    }

    // History Expansion
    if (buf[0] == '!') {
        char *target_cmd = 0;
        if (buf[1] == '!') {
            if (cmd_count > 1) target_cmd = history[(cmd_count - 2) % MAX_HISTORY].cmd;
        } else if (buf[1] >= '0' && buf[1] <= '9') {
            int target_id = atoi(&buf[1]);
            if (target_id > 0 && target_id < cmd_count && target_id >= cmd_count - MAX_HISTORY)
                target_cmd = history[(target_id - 1) % MAX_HISTORY].cmd; 
        } else {
            for (int i = cmd_count - 2; i >= 0 && i >= cmd_count - 1 - MAX_HISTORY; i--) {
                char *past = history[i % MAX_HISTORY].cmd;
                char *p1 = past; char *p2 = &buf[1];
                int match = 1;
                while (*p2) { if (*p1++ != *p2++) { match = 0; break; } }
                if (match) { target_cmd = past; break; }
            }
        }
        if (target_cmd) { printf("%s\n", target_cmd); strcpy(buf, target_cmd); }
        else { fprintf(2, "smash: event not found\n"); continue; }
    }

    // Tokenize
    char *args[MAX_ARGS]; 
    int tokens = 0;
    char raw_buf[MAX_CMD_LEN];
    strcpy(raw_buf, buf);

    char *next_tok = buf;
    char *curr_tok;
    while ((curr_tok = next_token(&next_tok, " \t\r\n")) != 0) {
      args[tokens++] = curr_tok;
      if(tokens >= MAX_ARGS - 1) break; 
    }
    args[tokens] = 0; 

    if (tokens == 0) continue;

    // Background Job Detection
    int is_background = 0;
    if (tokens > 0 && strcmp(args[tokens-1], "&") == 0) {
        is_background = 1;
        args[tokens-1] = 0; 
        tokens--;
        if(tokens == 0) continue; 
    }

    int is_history_cmd = (strcmp(args[0], "history") == 0);
    int current_hist_idx = -1;
    if (!is_history_cmd) {
        current_hist_idx = (cmd_count - 1) % MAX_HISTORY;
        strcpy(history[current_hist_idx].cmd, raw_buf);
        history[current_hist_idx].duration_ms = 0;
        cmd_count++;
    }

    int start_ticks = uptime();

    // --- Built-in Commands ---
    if (strcmp(args[0], "exit") == 0) {
      exit(0);
    }
    else if (is_history_cmd) {
        int show_time = (tokens > 1 && strcmp(args[1], "-t") == 0);
        int start = 1;
        if (cmd_count > MAX_HISTORY + 1) start = cmd_count - MAX_HISTORY;
        for (int i = start; i < cmd_count; i++) {
            struct history_entry *h = &history[(i - 1) % MAX_HISTORY];
            if (show_time) printf("[%d|%dms] %s\n", i, h->duration_ms, h->cmd);
            else printf("  %d %s\n", i, h->cmd);
        }
        last_status = 0;
    } 
    else if (strcmp(args[0], "cd") == 0) {
      if (tokens < 2) { printf("cd: argument missing\n"); last_status = 1; }
      else {
        if (chdir(args[1]) < 0) { printf("chdir: no such file or directory: %s\n", args[1]); last_status = 1; }
        else last_status = 0;
      }
    } 
    // --- External Commands (Pipeline Support) ---
    else {
        // 1. Identify Pipeline Segments
        int cmd_start_indices[MAX_ARGS];
        int num_cmds = 0;
        cmd_start_indices[num_cmds++] = 0;

        for(int i=0; i<tokens; i++){
            if(strcmp(args[i], "|") == 0){
                args[i] = 0; // Terminate current segment
                cmd_start_indices[num_cmds++] = i + 1;
            }
        }

        int prev_pipe_read = -1;
        int curr_pipe[2];
        int last_pid = 0;

        // 2. Loop through each command in the pipeline
        for(int i=0; i<num_cmds; i++){
            char **c_args = &args[cmd_start_indices[i]];
            
            if(i < num_cmds - 1){
                if(pipe(curr_pipe) < 0){ fprintf(2, "pipe failed\n"); break; }
            }

            int pid = fork();
            if(pid < 0){ fprintf(2, "fork failed\n"); break; }

            if(pid == 0){
                // === CHILD PROCESS ===
                if(prev_pipe_read != -1){
                    close(0); dup(prev_pipe_read); close(prev_pipe_read);
                }
                if(i < num_cmds - 1){
                    close(1); dup(curr_pipe[1]); close(curr_pipe[0]); close(curr_pipe[1]);
                }

                // Handle Redirection
                for(int j=0; c_args[j] != 0; j++){
                    char *redir = c_args[j];
                    char *fname = c_args[j+1];

                    if(strcmp(redir, "<") == 0 || strcmp(redir, ">") == 0 || strcmp(redir, ">>") == 0){
                         if(fname == 0){ fprintf(2, "syntax error\n"); exit(1); }
                         
                         if(strcmp(redir, "<") == 0){
                             close(0);
                             if(open(fname, O_RDONLY) < 0){ fprintf(2, "cannot open %s\n", fname); exit(1); }
                         } else if(strcmp(redir, ">") == 0){
                             close(1);
                             if(open(fname, O_WRONLY|O_CREATE|O_TRUNC) < 0){ fprintf(2, "cannot open %s\n", fname); exit(1); }
                         } else if(strcmp(redir, ">>") == 0){
                             close(1);
                             if(open(fname, O_WRONLY|O_CREATE|O_APPEND) < 0){ fprintf(2, "cannot open %s\n", fname); exit(1); }
                         }
                         c_args[j] = 0; 
                    }
                }

                // >>> Use new execvp logic here <<<
                execvp(c_args[0], c_args);
                
                // execvp handles exit(1) on failure, so we shouldn't reach here.
                exit(1);

            } else {
                // === PARENT PROCESS ===
                if(prev_pipe_read != -1) close(prev_pipe_read);
                if(i < num_cmds - 1){
                    close(curr_pipe[1]); 
                    prev_pipe_read = curr_pipe[0]; 
                }
                last_pid = pid; 
            }
        }
        
        if (!is_background) {
            int wpid;
            int status;
            while((wpid = wait(&status)) != -1){
                if(wpid == last_pid) last_status = status;
            }
        }
    }

    int end_ticks = uptime();
    if (!is_history_cmd && current_hist_idx >= 0) {
        history[current_hist_idx].duration_ms = (end_ticks - start_ticks) * 100;
    }
  } 
  exit(0);
}
