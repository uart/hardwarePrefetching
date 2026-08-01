echo "MSR 0x1A4"
OUTPUT="$(sudo rdmsr 0x1a4)"
./msr2settings a 0x1a4 "${OUTPUT}"

echo "MSR 0x1320"
OUTPUT="$(sudo rdmsr 0x1320)"
./msr2settings a 0x1320 "${OUTPUT}"

echo "\nMSR 0x1321"
OUTPUT="$(sudo rdmsr 0x1321)"
./msr2settings a 0x1321 "${OUTPUT}"

echo "\nMSR 0x1322"
OUTPUT="$(sudo rdmsr 0x1322)"
./msr2settings a 0x1322 "${OUTPUT}"

echo "\nMSR 0x1323"
OUTPUT="$(sudo rdmsr 0x1323)"
./msr2settings a 0x1323 "${OUTPUT}"

echo "\nMSR 0x1324"
OUTPUT="$(sudo rdmsr 0x1324)"
./msr2settings a 0x1324 "${OUTPUT}"

echo "\nMSR 0x1325"
OUTPUT="$(sudo rdmsr 0x1325)"
./msr2settings a 0x1324 "${OUTPUT}"

echo "\nMSR 0x1326"
OUTPUT="$(sudo rdmsr 0x1326)"
./msr2settings a 0x1324 "${OUTPUT}"

echo "\nMSR 0x1327"
OUTPUT="$(sudo rdmsr 0x1327)"
./msr2settings a 0x1324 "${OUTPUT}"


