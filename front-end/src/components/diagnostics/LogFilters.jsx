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
  const logLevels = [
    { value: 'all', label: 'All Levels' },
    { value: 'info', label: 'INFO' },
    { value: 'warning', label: 'WARNING' },
    { value: 'error', label: 'ERROR' },
    { value: 'debug', label: 'DEBUG' }
  ];

  const handleLevelChange = (event) => {
    onFilterChange({ ...filters, level: event.target.value });
  };

  const handleSearchChange = (event) => {
    onFilterChange({ ...filters, search: event.target.value });
  };

  const handleStartDateChange = (event) => {
    onFilterChange({ ...filters, startDate: event.target.value });
  };

  const handleEndDateChange = (event) => {
    onFilterChange({ ...filters, endDate: event.target.value });
  };

  const getActiveFilterCount = () => {
    let count = 0;
    if (filters.level && filters.level !== 'all') count++;
    if (filters.search) count++;
    if (filters.startDate) count++;
    if (filters.endDate) count++;
    return count;
  };

  const activeFilterCount = getActiveFilterCount();

  return (
    <Paper sx={{ p: 2, mb: 2 }}>
      <Grid container spacing={2} alignItems="center">
        {/* Log Level Filter */}
        <Grid item xs={12} sm={6} md={3}>
          <FormControl fullWidth size="small">
            <InputLabel>Log Level</InputLabel>
            <Select
              value={filters.level || 'all'}
              label="Log Level"
              onChange={handleLevelChange}
            >
              {logLevels.map((level) => (
                <MenuItem key={level.value} value={level.value}>
                  {level.label}
                </MenuItem>
              ))}
            </Select>
          </FormControl>
        </Grid>

        {/* Start Date Filter */}
        <Grid item xs={12} sm={6} md={2}>
          <TextField
            fullWidth
            size="small"
            type="datetime-local"
            label="Start Date"
            value={filters.startDate || ''}
            onChange={handleStartDateChange}
            InputLabelProps={{ shrink: true }}
          />
        </Grid>

        {/* End Date Filter */}
        <Grid item xs={12} sm={6} md={2}>
          <TextField
            fullWidth
            size="small"
            type="datetime-local"
            label="End Date"
            value={filters.endDate || ''}
            onChange={handleEndDateChange}
            InputLabelProps={{ shrink: true }}
          />
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
            Clear Filters
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
        <Grid item xs={12}>
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
            {filters.deviceId && filters.deviceId !== 'all' && (
              <Chip
                label={`Device: ${filters.deviceId}`}
                onDelete={() => onFilterChange({ ...filters, deviceId: 'all' })}
                size="small"
              />
            )}
            {filters.level && filters.level !== 'all' && (
              <Chip
                label={`Level: ${filters.level.toUpperCase()}`}
                onDelete={() => onFilterChange({ ...filters, level: 'all' })}
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
            {filters.startDate && (
              <Chip
                label={`From: ${new Date(filters.startDate).toLocaleString()}`}
                onDelete={() => onFilterChange({ ...filters, startDate: '' })}
                size="small"
              />
            )}
            {filters.endDate && (
              <Chip
                label={`To: ${new Date(filters.endDate).toLocaleString()}`}
                onDelete={() => onFilterChange({ ...filters, endDate: '' })}
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
