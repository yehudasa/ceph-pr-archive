export class ConfigOptionTypes {
  public static KNOWN_TYPES = [
    {
      name: 'uint64_t',
      inputType: 'number',
      humanReadable: 'Positive integer value',
      defaultMin: 0,
      patternHelpText: 'The entered value needs to be a positive number.',
      isNumberType: true,
      allowsNegative: false
    },
    {
      name: 'int64_t',
      inputType: 'number',
      humanReadable: 'Integer value',
      patternHelpText: 'The entered value needs to be a number.',
      isNumberType: true,
      allowsNegative: true
    },
    {
      name: 'size_t',
      inputType: 'number',
      humanReadable: 'Positive integer value (size)',
      defaultMin: 0,
      patternHelpText: 'The entered value needs to be a positive number.',
      isNumberType: true,
      allowsNegative: false
    },
    {
      name: 'secs',
      inputType: 'number',
      humanReadable: 'Positive integer value (secs)',
      defaultMin: 1,
      patternHelpText: 'The entered value needs to be a positive number.',
      isNumberType: true,
      allowsNegative: false
    },
    {
      name: 'double',
      inputType: 'number',
      humanReadable: 'Decimal value',
      patternHelpText: 'The entered value needs to be a number or decimal.',
      isNumberType: true,
      allowsNegative: true
    },
    { name: 'std::string', inputType: 'text', humanReadable: 'Text', isNumberType: false },
    {
      name: 'entity_addr_t',
      inputType: 'text',
      humanReadable: 'IPv4 or IPv6 address',
      patternHelpText: 'The entered value needs to be a valid IP address.',
      isNumberType: false
    },
    {
      name: 'uuid_d',
      inputType: 'text',
      humanReadable: 'UUID',
      patternHelpText:
        'The entered value is not a valid UUID, e.g.: 67dcac9f-2c03-4d6c-b7bd-1210b3a259a8',
      isNumberType: false
    },
    { name: 'bool', inputType: 'checkbox', humanReadable: 'Boolean value', isNumberType: false }
  ];
}
