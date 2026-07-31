#include "../include/api_emulator.h"
#include "../include/binary_to_instruction.h"
#include "../include/helper.h"
#include "../include/logger.h"
#include "../include/memory.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <time.h>
#include <unistd.h>

/**
 * @brief Encodes the outcome of executing a single decoded instruction
 */
typedef enum : uint8_t {
  EXEC_OK,
  EXEC_BRANCH_TAKEN,
  EXEC_ECALL,
  EXEC_EBREAK,
  EXEC_INVALID_OP,
  EXEC_MEM_READ_FAULT,
  EXEC_MEM_WRITE_FAULT,
} ExecResult;

/**
 * @brief Reads a line from stdin and parses it as an integer in the given base
 *
 * @param base The numeric base to parse with (e.g. 10 or 16)
 * @param out Where to write the parsed integer on success
 * @return int 1 if a valid integer was read, 0 if the line was not a valid integer (recoverable,
 * stdin's error state is cleared), -1 if stdin has reached EOF
 */
static int read_int_from_stdin(int base, int* out) {
  if (!out)
    return 0;

  char buf[16];
  char* end;

  if (fgets(buf, sizeof(buf), stdin) != NULL) {
    size_t len = strlen(buf);
    if (len == sizeof(buf) - 1 && buf[len - 1] != '\n') {
      int c;
      while ((c = fgetc(stdin)) != EOF && c != '\n')
        ;
    }

    errno = 0;
    long x = strtol(buf, &end, base);
    if (buf == end)
      return 0;
    if (errno == ERANGE)
      return 0;
    if (x < INT_MIN || x > INT_MAX)
      return 0;
    while (*end == '\n' || *end == '\r' || *end == ' ')
      end++;
    if (*end != '\0')
      return 0;

    *out = (int)x;
    return 1;
  }

  if (feof(stdin))
    return -1;
  clearerr(stdin);
  return 0;
}

/**
 * @brief Dispatches and executes the ecall identified by register a7, using a0/a1 as
 * arguments/return values
 *
 * @param prog The program state; may have its status changed to CPU_EXITED or CPU_HALTED
 * @return int 1 if the ecall was recognized and handled (including cases that halt or exit the
 * program), 0 if a7 is not a supported ecall number or the operation failed
 */
static int handle_ecall(ProgramState* prog) {
  if (!prog)
    return 0;

  uint32_t a7 = prog->registers[17];
  uint32_t* a0 = &prog->registers[10];
  uint32_t* a1 = &prog->registers[11];

  switch (a7) {
  case 1:
    fprintf(stdout, "%d", *a0);
    return 1;
  case 4:
    uint32_t curr_read = *a0;
    uint8_t c_read = 0;
    while (mem_read_u8(&prog->pt, curr_read++, &c_read) == ACCESS_OK && c_read != '\0')
      fprintf(stdout, "%c", (char)c_read);
    if (c_read != '\0')
      return 0;
    return 1;
  case 5:
    int x;
    {
      int result = read_int_from_stdin(10, &x);
      if (result == 0)
        result = read_int_from_stdin(16, &x);
      if (result > 0) {
        *a0 = x;
        return 1;
      }
      if (result < 0) {
        prog->status = CPU_HALTED;
        return 1;
      }
    }
    *a0 = 0;
    return 1;
  case 8:
    uint32_t curr_write = *a0;
    int c_write;
    int hit_eof = 0;
    while ((c_write = fgetc(stdin)) != EOF && c_write != '\n')
      if (mem_write_u8(&prog->pt, curr_write++, (uint8_t)c_write, MEMORY_READ | MEMORY_WRITE) != ACCESS_OK)
        return 0;
    if (c_write == EOF) {
      if (feof(stdin)) {
        hit_eof = 1;
      } else {
        clearerr(stdin);
        return 0;
      }
    }

    if (mem_write_u8(&prog->pt, curr_write, '\0', MEMORY_READ | MEMORY_WRITE) != ACCESS_OK)
      return 0;
    if (hit_eof)
      prog->status = CPU_HALTED;
    return 1;
  case 9:
    uint32_t bytes = *a0;
    uint32_t base = prog->heap_break;

    if (bytes == 0) {
      *a0 = base;
      return 1;
    }

    if (bytes > prog->stack_limit - base) {
      LOG_ERROR("sbrk request would overlap stack region");
      return 0;
    }

    static const uint8_t zero_page[PAGE_SIZE] = {0};
    uint32_t remaining = bytes;
    uint32_t current = base;
    while (remaining > 0) {
      uint32_t chunk = remaining > PAGE_SIZE ? PAGE_SIZE : remaining;
      if (mem_write_raw(&prog->pt, current, zero_page, chunk, MEMORY_READ | MEMORY_WRITE) != ACCESS_OK)
        return 0;
      current += chunk;
      remaining -= chunk;
    }

    prog->heap_break = base + bytes;
    *a0 = base;
    return 1;
  case 10:
    prog->status = CPU_EXITED;
    return 1;
  case 11:
    fprintf(stdout, "%c", *a0);
    return 1;
  case 12:
    int c = getchar();
    if (c == EOF) {
      if (feof(stdin)) {
        prog->status = CPU_HALTED;
        return 1;
      }
      clearerr(stdin);
      return 0;
    }
    *a0 = (uint32_t)c;
    return 1;
  case 17:
    prog->exit_code = *a0;
    prog->status = CPU_EXITED;
    return 1;
  case 30:
    struct timespec ts;
    if (timespec_get(&ts, TIME_UTC)) {
      uint64_t ms = ((uint64_t)ts.tv_sec * 1000) + ((uint64_t)ts.tv_nsec / 1000000);
      *a1 = ms >> 32;
      *a0 = (uint32_t)ms;
      return 1;
    }
    return 0;
  case 32:
    uint32_t ms = *a0;
    struct timespec duration = {.tv_sec = ms / 1000, .tv_nsec = (long)(ms % 1000) * 1000000L};
    thrd_sleep(&duration, NULL);
    return 1;
  case 34:
    fprintf(stdout, "%X", *a0);
    return 1;
  case 35:
    print_binary(*a0);
    return 1;
  case 36:
    fprintf(stdout, "%u", *a0);
    return 1;
  default:
    return 0;
  }

  return 0;
}

/**
 * @brief Executes a single decoded instruction, updating registers, memory, and pc as needed
 *
 * @param prog The program state to execute against
 * @param ins The decoded instruction to execute
 * @return ExecResult The outcome of execution
 */
static ExecResult handle_instruction(ProgramState* prog, DecodedInstruction* ins) {
#define WRITE_RD(val)                                                                                                  \
  do {                                                                                                                 \
    if (rd != 0)                                                                                                       \
      x[rd] = (uint32_t)(val);                                                                                         \
  } while (0)

  if (!prog || !ins)
    return EXEC_INVALID_OP;

  uint32_t* x = prog->registers;
  uint8_t rd = ins->rd;
  uint8_t rs1 = ins->rs1;
  uint8_t rs2 = ins->rs2;
  int32_t imm = ins->imm;
  uint32_t pc = prog->pc;

  switch (ins->op) {
  case OP_ADD: {
    WRITE_RD(x[rs1] + x[rs2]);
    return EXEC_OK;
  }

  case OP_SUB: {
    WRITE_RD(x[rs1] - x[rs2]);
    return EXEC_OK;
  }

  case OP_AND: {
    WRITE_RD(x[rs1] & x[rs2]);
    return EXEC_OK;
  }

  case OP_OR: {
    WRITE_RD(x[rs1] | x[rs2]);
    return EXEC_OK;
  }

  case OP_XOR: {
    WRITE_RD(x[rs1] ^ x[rs2]);
    return EXEC_OK;
  }

  case OP_SLT: {
    WRITE_RD((int32_t)x[rs1] < (int32_t)x[rs2] ? 1 : 0);
    return EXEC_OK;
  }

  case OP_SLTU: {
    WRITE_RD(x[rs1] < x[rs2] ? 1 : 0);
    return EXEC_OK;
  }

  case OP_SLL: {
    WRITE_RD(x[rs1] << (x[rs2] & 0x1F));
    return EXEC_OK;
  }

  case OP_SRL: {
    WRITE_RD(x[rs1] >> (x[rs2] & 0x1F));
    return EXEC_OK;
  }

  case OP_SRA: {
    WRITE_RD((int32_t)x[rs1] >> (x[rs2] & 0x1F));
    return EXEC_OK;
  }

  case OP_MUL: {
    WRITE_RD(x[rs1] * x[rs2]);
    return EXEC_OK;
  }

  case OP_MULH: {
    WRITE_RD(((int64_t)(int32_t)x[rs1] * (int64_t)(int32_t)x[rs2]) >> 32);
    return EXEC_OK;
  }

  case OP_MULHU: {
    WRITE_RD(((uint64_t)x[rs1] * (uint64_t)x[rs2]) >> 32);
    return EXEC_OK;
  }

  case OP_MULHSU: {
    WRITE_RD(((int64_t)(int32_t)x[rs1] * (int64_t)(uint64_t)x[rs2]) >> 32);
    return EXEC_OK;
  }

  case OP_DIV: {
    if (x[rs2] == 0)
      WRITE_RD(0xFFFFFFFFu);
    else if ((int32_t)x[rs1] == INT32_MIN && (int32_t)x[rs2] == -1)
      WRITE_RD((uint32_t)INT32_MIN);
    else
      WRITE_RD((int32_t)x[rs1] / (int32_t)x[rs2]);
    return EXEC_OK;
  }

  case OP_DIVU: {
    if (x[rs2] == 0)
      WRITE_RD(0xFFFFFFFFu);
    else
      WRITE_RD(x[rs1] / x[rs2]);
    return EXEC_OK;
  }

  case OP_REM: {
    if (x[rs2] == 0)
      WRITE_RD(x[rs1]);
    else if ((int32_t)x[rs1] == INT32_MIN && (int32_t)x[rs2] == -1)
      WRITE_RD(0);
    else
      WRITE_RD((int32_t)x[rs1] % (int32_t)x[rs2]);
    return EXEC_OK;
  }

  case OP_REMU: {
    if (x[rs2] == 0)
      WRITE_RD(x[rs1]);
    else
      WRITE_RD(x[rs1] % x[rs2]);
    return EXEC_OK;
  }

  case OP_ADDI: {
    WRITE_RD((int32_t)x[rs1] + imm);
    return EXEC_OK;
  }

  case OP_ANDI: {
    WRITE_RD(x[rs1] & (uint32_t)imm);
    return EXEC_OK;
  }

  case OP_ORI: {
    WRITE_RD(x[rs1] | (uint32_t)imm);
    return EXEC_OK;
  }

  case OP_XORI: {
    WRITE_RD(x[rs1] ^ (uint32_t)imm);
    return EXEC_OK;
  }

  case OP_SLTI: {
    WRITE_RD((int32_t)x[rs1] < imm ? 1 : 0);
    return EXEC_OK;
  }

  case OP_SLTIU: {
    WRITE_RD(x[rs1] < (uint32_t)imm ? 1 : 0);
    return EXEC_OK;
  }

  case OP_SLLI: {
    WRITE_RD(x[rs1] << (imm & 0x1F));
    return EXEC_OK;
  }

  case OP_SRLI: {
    WRITE_RD(x[rs1] >> (imm & 0x1F));
    return EXEC_OK;
  }

  case OP_SRAI: {
    WRITE_RD((int32_t)x[rs1] >> (imm & 0x1F));
    return EXEC_OK;
  }

  case OP_LB: {
    uint8_t val;
    if (mem_read_u8(&prog->pt, x[rs1] + imm, &val) != ACCESS_OK)
      return EXEC_MEM_READ_FAULT;
    WRITE_RD((int32_t)(int8_t)val);
    return EXEC_OK;
  }

  case OP_LH: {
    uint16_t val;
    if (mem_read_u16(&prog->pt, x[rs1] + imm, &val) != ACCESS_OK)
      return EXEC_MEM_READ_FAULT;
    WRITE_RD((int32_t)(int16_t)val);
    return EXEC_OK;
  }

  case OP_LW: {
    uint32_t val;
    if (mem_read_u32(&prog->pt, x[rs1] + imm, &val) != ACCESS_OK)
      return EXEC_MEM_READ_FAULT;
    WRITE_RD(val);
    return EXEC_OK;
  }

  case OP_LBU: {
    uint8_t val;
    if (mem_read_u8(&prog->pt, x[rs1] + imm, &val) != ACCESS_OK)
      return EXEC_MEM_READ_FAULT;
    WRITE_RD(val);
    return EXEC_OK;
  }

  case OP_LHU: {
    uint16_t val;
    if (mem_read_u16(&prog->pt, x[rs1] + imm, &val) != ACCESS_OK)
      return EXEC_MEM_READ_FAULT;
    WRITE_RD(val);
    return EXEC_OK;
  }

  case OP_SB: {
    if (mem_write_u8(&prog->pt, x[rs1] + imm, (uint8_t)x[rs2], MEMORY_WRITE) != ACCESS_OK)
      return EXEC_MEM_WRITE_FAULT;
    return EXEC_OK;
  }

  case OP_SH: {
    if (mem_write_u16(&prog->pt, x[rs1] + imm, (uint16_t)x[rs2], MEMORY_WRITE) != ACCESS_OK)
      return EXEC_MEM_WRITE_FAULT;
    return EXEC_OK;
  }

  case OP_SW: {
    if (mem_write_u32(&prog->pt, x[rs1] + imm, x[rs2], MEMORY_WRITE) != ACCESS_OK)
      return EXEC_MEM_WRITE_FAULT;
    return EXEC_OK;
  }

  case OP_BEQ: {
    if (x[rs1] == x[rs2]) {
      prog->pc = pc + imm;
      return EXEC_BRANCH_TAKEN;
    }
    return EXEC_OK;
  }

  case OP_BNE: {
    if (x[rs1] != x[rs2]) {
      prog->pc = pc + imm;
      return EXEC_BRANCH_TAKEN;
    }
    return EXEC_OK;
  }

  case OP_BLT: {
    if ((int32_t)x[rs1] < (int32_t)x[rs2]) {
      prog->pc = pc + imm;
      return EXEC_BRANCH_TAKEN;
    }
    return EXEC_OK;
  }

  case OP_BGE: {
    if ((int32_t)x[rs1] >= (int32_t)x[rs2]) {
      prog->pc = pc + imm;
      return EXEC_BRANCH_TAKEN;
    }
    return EXEC_OK;
  }

  case OP_BLTU: {
    if (x[rs1] < x[rs2]) {
      prog->pc = pc + imm;
      return EXEC_BRANCH_TAKEN;
    }
    return EXEC_OK;
  }

  case OP_BGEU: {
    if (x[rs1] >= x[rs2]) {
      prog->pc = pc + imm;
      return EXEC_BRANCH_TAKEN;
    }
    return EXEC_OK;
  }

  case OP_JAL: {
    WRITE_RD(pc + 4);
    prog->pc = pc + imm;
    return EXEC_BRANCH_TAKEN;
  }

  case OP_JALR: {
    uint32_t target = (x[rs1] + (uint32_t)imm) & ~1u;
    WRITE_RD(pc + 4);
    prog->pc = target;
    return EXEC_BRANCH_TAKEN;
  }

  case OP_LUI: {
    WRITE_RD(imm);
    return EXEC_OK;
  }

  case OP_AUIPC: {
    WRITE_RD(pc + imm);
    return EXEC_OK;
  }

  case OP_ECALL: {
    return EXEC_ECALL;
  }

  case OP_EBREAK: {
    return EXEC_EBREAK;
  }

  default: {
    return EXEC_INVALID_OP;
  }
  }

#undef WRITE_RD
}

StepResult cpu_step_single(ProgramState* prog) {
  StepResult r = {.pc_old = prog->pc};

  MemoryAccessResult fr = mem_fetch_instruction(&prog->pt, prog->pc, &r.raw);
  if (fr != ACCESS_OK) {
    r.status = STEP_HALTED_FETCH;
    prog->status = CPU_HALTED;
    return r;
  }

  if (!binary_to_instruction(r.raw, &r.decoded)) {
    r.status = STEP_HALTED_DECODE;
    prog->status = CPU_HALTED;
    return r;
  }

  ExecResult h = handle_instruction(prog, &r.decoded);
  switch (h) {
  case EXEC_OK:
    r.status = STEP_OK;
    prog->pc += 4;
    break;
  case EXEC_BRANCH_TAKEN:
    r.status = STEP_BRANCH_TAKEN;
    break;
  case EXEC_ECALL:
    if (!handle_ecall(prog)) {
      prog->status = CPU_HALTED;
      r.status = STEP_HALTED_ECALL_INVALID;
    } else {
      if (prog->status == CPU_EXITED) {
        r.status = STEP_EXITED;
      } else if (prog->status == CPU_HALTED) {
        r.status = STEP_HALTED_ECALL_INVALID;
      } else {
        prog->pc += 4;
        r.status = STEP_ECALL_HANDLED;
      }
    }
    break;
  case EXEC_EBREAK:
    r.status = STEP_EBREAK;
    break;
  case EXEC_INVALID_OP:
    prog->status = CPU_HALTED;
    r.status = STEP_HALTED_INVALID_OP;
    break;
  case EXEC_MEM_READ_FAULT:
    prog->status = CPU_HALTED;
    r.status = STEP_HALTED_MEM_READ;
    break;
  case EXEC_MEM_WRITE_FAULT:
    prog->status = CPU_HALTED;
    r.status = STEP_HALTED_MEM_WRITE;
    break;
  }

  r.pc_new = prog->pc;
  return r;
}

int step_status_is_fault(StepStatus s) {
  switch (s) {
  case STEP_HALTED_FETCH:
  case STEP_HALTED_DECODE:
  case STEP_HALTED_INVALID_OP:
  case STEP_HALTED_MEM_READ:
  case STEP_HALTED_MEM_WRITE:
  case STEP_HALTED_ECALL_INVALID:
    return 1;
  default:
    return 0;
  }
}