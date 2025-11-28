/**
 * LogFilters Component
 * 
 * Provides filtering options for diagnostic logs
 * Features:
 * - Device filter
 * - Log level filter (INFO, WARNING, ERROR)
 * - Date range filter
 * - Search text filter
 * - Clear all filters button
 */

import React from 'react';
import {
  Box,
  Paper,
  FormControl,
  InputLabel,
  Select,
  MenuItem,
  TextField,
  Button,
  Grid,
  Chip,
  Stack
} from '@mui/material';
import {
  Clear as ClearIcon,
  Search as SearchIcon
} from '@mui/icons-material';

const LogFilters = ({ 
  filters, 
  onFilterChange, 
  onClearFilters 
}) => {
  const logTypes = [
    { value: 'all', label: 'All Types' },
    { value: 'DATA_UPLOAD', label: '📊 Data Upload' },
    { value: 'COMMAND', label: '⚡ Command' },
    { value: 'CONFIG_CHANGE', label: '⚙️ Config Change' },
    { value: 'OTA_UPDATE', label: '🔄 OTA Update' },
    { value: 'FAULT_INJECTION', label: '🧪 Fault Test' },
    { value: 'FAULT_RECOVERY', label: '🔧 Recovery' }
  ];

  const handleTypeChange = (event) => {
    onFilterChange({ ...filters, type: event.target.value });
  };

  const handleSearchChange = (event) => {
    onFilterChange({ ...filters, search: event.target.value });
  };

  const getActiveFilterCount = () => {
    let count = 0;
    if (filters.type && filters.type !== 'all') count++;
    if (filters.search) count++;
    return count;
  };

  const activeFilterCount = getActiveFilterCount();

  return (
    <Paper sx={{ p: 2, mb: 2 }}>
      <Grid container spacing={2} alignItems="center">
        {/* Log Type Filter */}
        <Grid item xs={12} sm={6} md={4}>
          <FormControl fullWidth size="small">
            <InputLabel>Activity Type</InputLabel>
            <Select
              value={filters.type || 'all'}
              label="Activity Type"
              onChange={handleTypeChange}
            >
              {logTypes.map((type) => (
                <MenuItem key={type.value} value={type.value}>
                  {type.label}
                </MenuItem>
              ))}
            </Select>
          </FormControl>
        </Grid>

        {/* Clear Filters Button */}
        <Grid item xs={12} sm={6} md={2}>
          <Button
            fullWidth
            variant="outlined"
            onClick={onClearFilters}
            disabled={activeFilterCount === 0}
            startIcon={<ClearIcon />}
          >
            Clear
            {activeFilterCount > 0 && (
              <Chip 
                label={activeFilterCount} 
                size="small" 
                color="primary"
                sx={{ ml: 1 }}
              />
            )}
          </Button>
        </Grid>

        {/* Search Filter */}
        <Grid item xs={12} md={6}>
          <TextField
            fullWidth
            size="small"
            placeholder="Search logs..."
            value={filters.search || ''}
            onChange={handleSearchChange}
            InputProps={{
              startAdornment: <SearchIcon sx={{ mr: 1, color: 'text.secondary' }} />
            }}
          />
        </Grid>
      </Grid>

      {/* Active Filters Display */}
      {activeFilterCount > 0 && (
        <Box sx={{ mt: 2 }}>
          <Stack direction="row" spacing={1} flexWrap="wrap">
            {filters.type && filters.type !== 'all' && (
              <Chip
                label={`Type: ${filters.type}`}
                onDelete={() => onFilterChange({ ...filters, type: 'all' })}
                size="small"
              />
            )}
            {filters.search && (
              <Chip
                label={`Search: "${filters.search}"`}
                onDelete={() => onFilterChange({ ...filters, search: '' })}
                size="small"
              />
            )}
          </Stack>
        </Box>
      )}
    </Paper>
  );
};

export default LogFilters;
