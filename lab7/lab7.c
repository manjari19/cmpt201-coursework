// lab7.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INPUT 100

typedef struct {
    int line_number;
    int value;
} Input;

typedef struct {
    int line_number;
    int doubled_value;
} IntermediateInput;

typedef struct {
    int doubled_value;
    int line_numbers[MAX_INPUT];
    int count;
} Output;

void map(Input* input, IntermediateInput* intermediate_input);
void groupByKey(IntermediateInput* input, Output *output, int *result_count);
void reduce(Output* output);

int main(void) {
    Input input_data[MAX_INPUT];
    int input_size = 0;

    printf("Enter values (one per line). Type 'end' to finish:\n");
    while (input_size < MAX_INPUT) {
        char buffer[100];
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) break;

        size_t len = strlen(buffer);
        if (len && buffer[len - 1] == '\n') buffer[len - 1] = '\0';

        if (strcmp(buffer, "end") == 0) break;

        char *endp = NULL;
        long v = strtol(buffer, &endp, 10);
        if (endp != buffer && *endp == '\0') {
            input_data[input_size].line_number = input_size + 1;
            input_data[input_size].value = (int)v;
            input_size++;
        } else {
            printf("Invalid input. Please enter an integer or 'end' to finish.\n");
        }
    }

    IntermediateInput mapped_results[MAX_INPUT] = {0};
    for (int i = 0; i < input_size; i++) {
        map(&input_data[i], &mapped_results[i]);
    }

    Output output_results[MAX_INPUT] = {0};
    int result_count = 0;
    for (int i = 0; i < input_size; i++) {
        groupByKey(&mapped_results[i], output_results, &result_count);
    }

    for (int i = 0; i < result_count; i++) {
        if (output_results[i].count > 0) {
            reduce(&output_results[i]);
        }
    }

    return 0;
}

void map(Input* input, IntermediateInput *intermediate_input) {
    intermediate_input->line_number = input->line_number;
    intermediate_input->doubled_value = input->value * 2;
}

void groupByKey(IntermediateInput* input, Output *output, int *result_count) {
    //finding exusting grp
    for (int i = 0; i < *result_count; i++) {
        if (output[i].doubled_value == input->doubled_value) {
            if (output[i].count < MAX_INPUT) {
                output[i].line_numbers[output[i].count++] = input->line_number;
            }
            return;
        }
    }

    //create a new group
    if (*result_count < MAX_INPUT) {
        int idx = *result_count;
        output[idx].doubled_value = input->doubled_value;
        output[idx].count = 0;
        if (output[idx].count < MAX_INPUT) {
            output[idx].line_numbers[output[idx].count++] = input->line_number;
        }
        (*result_count)++;
    }
}

void reduce(Output* output) {
    printf("(%d, [", output->doubled_value);
    for (int i = 0; i < output->count; i++) {
        if (i > 0) printf(", ");
        printf("%d", output->line_numbers[i]);
    }
    printf("])\n");
}

