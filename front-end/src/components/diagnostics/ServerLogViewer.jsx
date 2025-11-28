/**
 * ServerLogViewer Component
 * 
 * Displays Flask server logs with filtering and search
 * Features:
 * - Real-time log streaming
 * - Color coding by severity
 * - Search functionality
 * - Level filtering
 * - Auto-refresh option
 */

import React, { useState } from 'react';
import {
  Box,
  Paper,
  Typography,
  List,
  ListItem,
  ListItemText,
  CircularProgress,
  Alert,
  Button,
  Switch,
  FormControlLabel,
  Stack,
  Chip,
  TextField,
  Select,
  MenuItem,
  FormControl,
  InputLabel,
  Divider
} from '@mui/material';
import {
  Refresh as RefreshIcon,
  Download as DownloadIcon
} from '@mui/icons-material';
import { useQuery } from '@tanstack/react-query';
import { getServerLogs } from '../../api/utilities';

const ServerLogViewer = () => {
  const [filters, setFilters] = useState({
    level: 'all',
    search: '',
    limit: 100
  });
  const [autoRefresh, setAutoRefresh] = useState(false);

  // Fetch logs
  const {
    data: logsData,
    isLoading,
    isError,
    error,
    refetch
  } = useQuery({
    queryKey: ['serverLogs', filters],
    queryFn: () => getServerLogs(filters.limit, filters.level, filters.search),
    refetchInterval: autoRefresh ? 3000 : false,
    staleTime: 2000
  });

  const logs = logsData?.data?.logs || [];

  const handleRefresh = () => {
    refetch();
  };

  const handleLevelChange = (event) => {
    setFilters({ ...filters, level: event.target.value });
  };

  const handleSearchChange = (event) => {
    setFilters({ ...filters, search: event.target.value });
  };

  const handleExport = () => {
    const logsText = logs.map(log => log.message).join('\n');
    const blob = new Blob([logsText], { type: 'text/plain' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `server_logs_${new Date().toISOString()}.txt`;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  };

  const getLevelColor = (level) => {
    switch (level.toUpperCase()) {
      case 'ERROR':
        return 'error';
      case 'WARNING':
        return 'warning';
      case 'SUCCESS':
        return 'success';
      case 'DEBUG':
        return 'info';
      default:
        return 'default';
    }
  };

  const getLevelTextColor = (level) => {
    switch (level.toUpperCase()) {
      case 'ERROR':
        return '#d32f2f';
      case 'WARNING':
        return '#ed6c02';
      case 'SUCCESS':
        return '#2e7d32';
      case 'DEBUG':
        return '#0288d1';
      default:
        return 'inherit';
    }
  };

  return (
    <Box>
      <Paper sx={{ p: 3 }}>
        <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', mb: 3 }}>
          <Typography variant="h6" sx={{ fontWeight: 600 }}>
            Flask Server Logs
          </Typography>
          <Stack direction="row" spacing={2}>
            <FormControlLabel
              control={
                <Switch
                  checked={autoRefresh}
                  onChange={(e) => setAutoRefresh(e.target.checked)}
                  size="small"
                />
              }
              label="Auto-refresh"
            />
            <Button
              variant="outlined"
              size="small"
              startIcon={<RefreshIcon />}
              onClick={handleRefresh}
              disabled={isLoading}
            >
              Refresh
            </Button>
            <Button
              variant="outlined"
              size="small"
              startIcon={<DownloadIcon />}
              onClick={handleExport}
              disabled={logs.length === 0}
            >
              Export
            </Button>
          </Stack>
        </Box>

        {/* Filters */}
        <Stack direction="row" spacing={2} sx={{ mb: 3 }}>
          <FormControl size="small" sx={{ minWidth: 150 }}>
            <InputLabel>Log Level</InputLabel>
            <Select
              value={filters.level}
              label="Log Level"
              onChange={handleLevelChange}
            >
              <MenuItem value="all">All Levels</MenuItem>
              <MenuItem value="info">INFO</MenuItem>
              <MenuItem value="success">SUCCESS</MenuItem>
              <MenuItem value="warning">WARNING</MenuItem>
              <MenuItem value="error">ERROR</MenuItem>
              <MenuItem value="debug">DEBUG</MenuItem>
            </Select>
          </FormControl>

          <TextField
            size="small"
            label="Search"
            placeholder="Search logs..."
            value={filters.search}
            onChange={handleSearchChange}
            sx={{ flexGrow: 1 }}
          />

          <FormControl size="small" sx={{ minWidth: 100 }}>
            <InputLabel>Limit</InputLabel>
            <Select
              value={filters.limit}
              label="Limit"
              onChange={(e) => setFilters({ ...filters, limit: e.target.value })}
            >
              <MenuItem value={50}>50</MenuItem>
              <MenuItem value={100}>100</MenuItem>
              <MenuItem value={200}>200</MenuItem>
              <MenuItem value={500}>500</MenuItem>
            </Select>
          </FormControl>
        </Stack>

        <Divider sx={{ mb: 2 }} />

        {/* Loading State */}
        {isLoading && (
          <Box sx={{ display: 'flex', justifyContent: 'center', py: 4 }}>
            <CircularProgress />
          </Box>
        )}

        {/* Error State */}
        {isError && (
          <Alert severity="error" sx={{ mb: 2 }}>
            Error loading logs: {error?.message || 'Unknown error'}
          </Alert>
        )}

        {/* Logs Display */}
        {!isLoading && !isError && (
          <>
            <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', mb: 2 }}>
              <Typography variant="body2" color="text.secondary">
                Showing {logs.length} log entries
              </Typography>
              {autoRefresh && (
                <Chip
                  label="Live"
                  color="success"
                  size="small"
                  sx={{ animation: 'pulse 2s infinite' }}
                />
              )}
            </Box>

            {logs.length === 0 ? (
              <Alert severity="info">
                No logs found matching the current filters.
              </Alert>
            ) : (
              <Paper 
                variant="outlined" 
                sx={{ 
                  maxHeight: 600, 
                  overflow: 'auto',
                  bgcolor: '#1e1e1e',
                  color: '#d4d4d4'
                }}
              >
                <List dense sx={{ p: 0 }}>
                  {logs.map((log, index) => (
                    <ListItem
                      key={index}
                      sx={{
                        borderBottom: index < logs.length - 1 ? '1px solid #333' : 'none',
                        py: 1,
                        px: 2,
                        fontFamily: 'monospace',
                        fontSize: '0.875rem',
                        '&:hover': {
                          bgcolor: '#2a2a2a'
                        }
                      }}
                    >
                      <Stack direction="row" spacing={1} sx={{ width: '100%' }} alignItems="flex-start">
                        <Chip
                          label={log.level}
                          size="small"
                          color={getLevelColor(log.level)}
                          sx={{ 
                            minWidth: 80,
                            fontWeight: 600,
                            fontSize: '0.75rem'
                          }}
                        />
                        <Typography
                          variant="body2"
                          sx={{
                            fontFamily: 'monospace',
                            fontSize: '0.875rem',
                            color: getLevelTextColor(log.level),
                            wordBreak: 'break-all',
                            whiteSpace: 'pre-wrap',
                            flexGrow: 1
                          }}
                        >
                          {log.message}
                        </Typography>
                      </Stack>
                    </ListItem>
                  ))}
                </List>
              </Paper>
            )}
          </>
        )}
      </Paper>

      <style>
        {`
          @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.5; }
          }
        `}
      </style>
    </Box>
  );
};

export default ServerLogViewer;
