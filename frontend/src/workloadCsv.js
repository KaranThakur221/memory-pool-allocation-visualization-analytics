export const MAX_WORKLOAD_IMPORT_BYTES = 1024 * 1024;

function csvError(line, message) {
  return new Error(`CSV line ${line}: ${message}`);
}

function readCsvRows(text) {
  const rows = [];
  let row = [];
  let field = '';
  let line = 1;
  let rowLine = 1;
  let inQuotes = false;
  let closedQuote = false;

  const finishField = () => {
    row.push(field);
    field = '';
    closedQuote = false;
  };
  const finishRow = () => {
    finishField();
    if (row.some((value) => value.trim() !== '')) rows.push({ line: rowLine, values: row });
    row = [];
    rowLine = line + 1;
  };

  for (let index = 0; index < text.length; index += 1) {
    const char = text[index];
    if (inQuotes) {
      if (char === '"') {
        if (text[index + 1] === '"') {
          field += '"';
          index += 1;
        } else {
          inQuotes = false;
          closedQuote = true;
        }
      } else {
        field += char;
        if (char === '\n') line += 1;
      }
      continue;
    }

    if (closedQuote && char !== ',' && char !== '\r' && char !== '\n' && !/\s/.test(char)) {
      throw csvError(line, 'unexpected character after a quoted field');
    }
    if (char === '"') {
      if (field.length !== 0 || closedQuote) throw csvError(line, 'quotes must enclose an entire field');
      inQuotes = true;
    } else if (char === ',') {
      finishField();
    } else if (char === '\n' || char === '\r') {
      if (char === '\r' && text[index + 1] === '\n') index += 1;
      finishRow();
      line += 1;
      rowLine = line;
    } else if (!closedQuote) {
      field += char;
    }
  }

  if (inQuotes) throw csvError(rowLine, 'unterminated quoted field');
  if (field.length > 0 || row.length > 0 || closedQuote) {
    finishRow();
  } else if (text.endsWith(',')) {
    finishField();
    rows.push({ line: rowLine, values: row });
  }
  return rows;
}

function positiveSafeInteger(value, label, line) {
  if (!/^\d+$/.test(value)) throw csvError(line, `${label} must be a positive whole number`);
  const number = Number(value);
  if (!Number.isSafeInteger(number) || number <= 0) {
    throw csvError(line, `${label} must be a positive safe integer`);
  }
  return number;
}

function validateOperations(operations, lineNumbers, isCsv = false) {
  const active = new Set();
  let allocationCount = 0;
  const fail = (index, message) => {
    if (isCsv) throw csvError(lineNumbers[index], message);
    throw new Error(`Workload operation ${index + 1}: ${message}.`);
  };

  return operations.map((entry, index) => {
    if (!entry || typeof entry !== 'object' || Array.isArray(entry)) {
      fail(index, 'operation must be an object');
    }
    const operation = typeof entry.operation === 'string' ? entry.operation.trim().toLowerCase() : '';
    if (operation !== 'allocate' && operation !== 'free') {
      fail(index, 'operation must be allocate or free');
    }

    if (operation === 'allocate') {
      if (!Number.isSafeInteger(entry.size) || entry.size <= 0) {
        fail(index, 'allocation size must be a positive safe integer');
      }
      allocationCount += 1;
      let allocationId;
      if (entry.allocation_id !== undefined && entry.allocation_id !== null && entry.allocation_id !== '') {
        allocationId = entry.allocation_id;
        if (!Number.isSafeInteger(allocationId) || allocationId <= 0) {
          fail(index, 'allocation_id must be a positive safe integer');
        }
        if (active.has(allocationId)) {
          fail(index, `allocation_id ${allocationId} is already active`);
        }
      } else {
        allocationId = allocationCount;
        while (active.has(allocationId)) allocationId += 1;
      }
      active.add(allocationId);
      return {
        operation,
        size: entry.size,
        ...(entry.allocation_id === undefined || entry.allocation_id === null || entry.allocation_id === ''
          ? {}
          : { allocation_id: allocationId }),
      };
    }

    if (entry.size !== undefined && entry.size !== null && entry.size !== '') {
      fail(index, 'free operations must not include a size');
    }
    const allocationId = entry.allocation_id;
    if (!Number.isSafeInteger(allocationId) || allocationId <= 0) {
      fail(index, 'free requires a positive allocation_id');
    }
    if (!active.has(allocationId)) {
      fail(index, `allocation_id ${allocationId} has no earlier active allocation`);
    }
    active.delete(allocationId);
    return { operation, allocation_id: allocationId };
  });
}

export function parseWorkloadText(text) {
  const input = text.trim();
  if (!input) throw new Error('Workload is empty.');

  if (input.startsWith('[')) {
    let parsed;
    try {
      parsed = JSON.parse(input);
    } catch (error) {
      throw new Error(`Invalid workload JSON: ${error.message}`);
    }
    if (!Array.isArray(parsed)) throw new Error('Workload JSON must be an array.');
    return validateOperations(parsed, parsed.map((_, index) => index + 1));
  }

  const rows = readCsvRows(text);
  if (rows.length === 0) throw new Error('CSV contains no header or workload rows.');
  const header = rows[0].values.map((value) => value.trim().replace(/^\uFEFF/, '').toLowerCase());
  if (header.length !== 3 || header[0] !== 'operation' || header[1] !== 'size' || header[2] !== 'allocation_id') {
    throw csvError(rows[0].line, 'expected header: operation,size,allocation_id');
  }

  const operations = [];
  const lineNumbers = [];
  for (const row of rows.slice(1)) {
    if (row.values.length !== 3) {
      throw csvError(row.line, 'expected exactly 3 columns (operation,size,allocation_id)');
    }
    const [rawOperation, rawSize, rawId] = row.values.map((value) => value.trim());
    const operation = rawOperation.toLowerCase();
    if (operation === 'allocate') {
      if (!rawSize) throw csvError(row.line, 'allocate requires a size');
      operations.push({
        operation,
        size: positiveSafeInteger(rawSize, 'size', row.line),
        ...(rawId ? { allocation_id: positiveSafeInteger(rawId, 'allocation_id', row.line) } : {}),
      });
    } else if (operation === 'free') {
      if (rawSize) throw csvError(row.line, 'free must not include a size');
      if (!rawId) throw csvError(row.line, 'free requires an allocation_id');
      operations.push({ operation, allocation_id: positiveSafeInteger(rawId, 'allocation_id', row.line) });
    } else {
      throw csvError(row.line, 'operation must be allocate or free');
    }
    lineNumbers.push(row.line);
  }
  if (operations.length === 0) throw new Error('CSV contains no workload operations.');
  return validateOperations(operations, lineNumbers, true);
}

export function workloadToCsv(workload) {
  const validated = validateOperations(workload, workload.map((_, index) => index + 1));
  const rows = ['operation,size,allocation_id'];
  for (const entry of validated) {
    rows.push(entry.operation === 'allocate'
      ? `allocate,${entry.size},${entry.allocation_id ?? ''}`
      : `free,,${entry.allocation_id}`);
  }
  return rows.join('\n');
}

export const WORKLOAD_CSV_TEMPLATE = [
  'operation,size,allocation_id',
  'allocate,120,',
  'allocate,240,',
  'free,,1',
].join('\n');
