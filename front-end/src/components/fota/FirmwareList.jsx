/**
 * FirmwareList Component
 * 
 * Displays available firmware versions
 * Features:
 * - List of firmware versions with metadata
 * - Delete firmware option
 * - View manifest
 * 
 * Note: ESP32 devices automatically poll for updates every 60 minutes.
 * Manual OTA initiation is not required.
 */

import React, { useState } from 'react';
import {
  Box,
  Paper,
  Typography,
  Table,
  TableBody,
  TableCell,
  TableContainer,
  TableHead,
  TableRow,
  Button,
  IconButton,
  Chip,
  Alert,
  CircularProgress,
  Dialog,
  DialogTitle,
  DialogContent,
  DialogActions,
  Tooltip,
  Stack
} from '@mui/material';
import {
  Delete as DeleteIcon,
  Visibility as ViewIcon,
  Refresh as RefreshIcon
} from '@mui/icons-material';
import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { getFirmwareList, deleteFirmware, getFirmwareManifest } from '../../api/ota';

const FirmwareList = () => {
  const [manifestDialogOpen, setManifestDialogOpen] = useState(false);
  const [manifestData, setManifestData] = useState(null);
  
  const queryClient = useQueryClient();

  // Fetch firmware list
  const {
    data: firmwareData,
    isLoading,
    isError,
    error,
    refetch
  } = useQuery({
    queryKey: ['firmwareList'],
    queryFn: getFirmwareList,
    staleTime: 10000
  });

  const firmwareList = firmwareData?.data?.firmwares || [];  // Fixed: was 'firmware', should be 'firmwares'

  // Delete firmware mutation
  const deleteMutation = useMutation({
    mutationFn: (version) => deleteFirmware(version),
    onSuccess: () => {
      queryClient.invalidateQueries(['firmwareList']);
    }
  });

  const handleDelete = (version) => {
    if (window.confirm(`Are you sure you want to delete firmware version ${version}?`)) {
      deleteMutation.mutate(version);
    }
  };

  const handleViewManifest = async (version) => {
    try {
      const response = await getFirmwareManifest(version);
      setManifestData(response.data);
      setManifestDialogOpen(true);
    } catch (err) {
      console.error('Error loading manifest:', err);
    }
  };

  const formatFileSize = (bytes) => {
    if (!bytes) return 'N/A';
    const k = 1024;
    const sizes = ['Bytes', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return Math.round(bytes / Math.pow(k, i) * 100) / 100 + ' ' + sizes[i];
  };

  const formatDate = (timestamp) => {
    if (!timestamp) return 'N/A';
    return new Date(timestamp * 1000).toLocaleString();
  };

  return (
    <Paper sx={{ p: 3 }}>
      <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', mb: 3 }}>
        <Typography variant="h6">
          Available Firmware Versions
        </Typography>
        <Tooltip title="Refresh list">
          <IconButton onClick={() => refetch()} color="primary">
            <RefreshIcon />
          </IconButton>
        </Tooltip>
      </Box>

      {/* Loading State */}
      {isLoading && (
        <Box sx={{ display: 'flex', justifyContent: 'center', py: 4 }}>
          <CircularProgress />
        </Box>
      )}

      {/* Error State */}
      {isError && (
        <Alert severity="error">
          Error loading firmware list: {error?.message || 'Unknown error'}
        </Alert>
      )}

      {/* Empty State */}
      {!isLoading && !isError && firmwareList.length === 0 && (
        <Alert severity="info">
          No firmware versions available. Upload a firmware file to get started.
        </Alert>
      )}

      {/* Firmware Table */}
      {!isLoading && !isError && firmwareList.length > 0 && (
        <TableContainer>
          <Table>
            <TableHead>
              <TableRow>
                <TableCell>Version</TableCell>
                <TableCell>Size</TableCell>
                <TableCell>Uploaded</TableCell>
                <TableCell>Status</TableCell>
                <TableCell align="center">Actions</TableCell>
              </TableRow>
            </TableHead>
            <TableBody>
              {firmwareList.map((firmware) => (
                <TableRow key={firmware.version} hover>
                  <TableCell>
                    <Typography variant="body2" fontWeight="bold">
                      {firmware.version}
                    </Typography>
                  </TableCell>
                  <TableCell>
                    {formatFileSize(firmware.size)}
                  </TableCell>
                  <TableCell>
                    {formatDate(firmware.uploaded_at)}
                  </TableCell>
                  <TableCell>
                    <Chip 
                      label={firmware.status || 'ready'}
                      color={firmware.status === 'ready' ? 'success' : 'default'}
                      size="small"
                    />
                  </TableCell>
                  <TableCell align="center">
                    <Stack direction="row" spacing={1} justifyContent="center">
                      <Tooltip title="View Manifest">
                        <IconButton
                          size="small"
                          onClick={() => handleViewManifest(firmware.version)}
                          color="info"
                        >
                          <ViewIcon />
                        </IconButton>
                      </Tooltip>
                      
                      <Tooltip title="Delete">
                        <IconButton
                          size="small"
                          onClick={() => handleDelete(firmware.version)}
                          color="error"
                          disabled={deleteMutation.isPending}
                        >
                          <DeleteIcon />
                        </IconButton>
                      </Tooltip>
                    </Stack>
                  </TableCell>
                </TableRow>
              ))}
            </TableBody>
          </Table>
        </TableContainer>
      )}

      {/* Manifest Dialog */}
      <Dialog open={manifestDialogOpen} onClose={() => setManifestDialogOpen(false)} maxWidth="md" fullWidth>
        <DialogTitle>Firmware Manifest</DialogTitle>
        <DialogContent>
          {manifestData && (
            <Box component="pre" sx={{ p: 2, bgcolor: 'grey.100', borderRadius: 1, overflow: 'auto' }}>
              {JSON.stringify(manifestData, null, 2)}
            </Box>
          )}
        </DialogContent>
        <DialogActions>
          <Button onClick={() => setManifestDialogOpen(false)}>Close</Button>
        </DialogActions>
      </Dialog>
    </Paper>
  );
};

export default FirmwareList;
