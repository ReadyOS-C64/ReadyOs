/* Keep the cc65 CRT entry resident while the production UI remains cold. */
int uz_cold_start(void);

#pragma code-name(push, "COLD_CODE")
int main(void) {
    return uz_cold_start();
}
#pragma code-name(pop)
