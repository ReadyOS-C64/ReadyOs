# ReadyBASIC Sound Phase 1 Lessons

- New command syntax should prefer existing parser signatures unless there is a very strong reason to grow resident code.
- For SID, packed bytes are not merely a compromise: `AD` and `SR` match the hardware registers and reduce command dispatch/expression overhead at 1 MHz.
- Sound verification cannot rely on screenshots. The automated suite should prove ReadyOS/ReadyBASIC loading, stored BASIC tokenization, command dispatch, and no BASIC error; human listening remains the real audio validation for this phase.
- Keep playback blocking in Phase 1. Nonblocking music implies persistent timing state and should be designed as a later poll/IRQ phase.
