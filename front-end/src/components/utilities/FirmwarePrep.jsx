/**
 * FirmwarePrep Component
 * 
 * Tool for preparing firmware files with manifest generation
 * Features:
 * - Firmware file upload
 * - Version input
 * - Algorithm selector
 * - Manifest generation and display
 * - Download prepared files
 */

import React, { useState } from 'react';
import {
  Box,
  Paper,
  Typography,
  Button,
  TextField,
  FormControl,
  InputLabel,
  Select,
  MenuItem,
  Alert,
  CircularProgress,
  Divider,
  Chip,
  Stack
} from '@mui/material';
import {
  CloudUpload as UploadIcon,
  PlayArrow as GenerateIcon,
  Download as DownloadIcon,
  Delete as ClearIcon
} from '@mui/icons-material';
import { useMutation } from '@tanstack/react-query';
import { prepareFirmware } from '../../api/utilities';

const FirmwarePrep = () => {
  const [file, setFile] = useState(null);
  const [version, setVersion] = useState('1.0.0');
  const [algorithm, setAlgorithm] = useState('zlib');
  const [manifest, setManifest] = useState(null);

  const algorithms = [
    { value: 'zlib', label: 'ZLIB (Default)' },
    { value: 'gzip', label: 'GZIP' },
    { value: 'lz4', label: 'LZ4 (Fast)' },
    { value: 'none', label: 'None (No Compression)' }
  ];

  const prepareMutation = useMutation({
    mutationFn: () => prepareFirmware(file, version, algorithm),
    onSuccess: (response) => {
      setManifest(response.data);
    }
  });

  const handleFileChange = (event) => {
    const selectedFile = event.target.files[0];
    if (selectedFile) {
      setFile(selectedFile);
      setManifest(null); // Clear previous manifest
    }
  };

  const handlePrepare = () => {
    if (!file) {
      return;
    }
    prepareMutation.mutate();
  };

  const handleClear = () => {
    setFile(null);
    setVersion('1.0.0');
    setAlgorithm('zlib');
    setManifest(null);
  };

  const downloadManifest = () => {
    if (!manifest || !manifest.manifest) return;

    const dataStr = JSON.stringify(manifest.manifest, null, 2);
    const dataBlob = new Blob([dataStr], { type: 'application/json' });
    const url = URL.createObjectURL(dataBlob);
    const link = document.createElement('a');
    link.href = url;
    link.download = `firmware_${version}_manifest.json`;
    link.click();
  };

  const getFileSize = () => {
    if (!file) return 'N/A';
    const size = file.size;
    if (size < 1024) return `${size} B`;
    if (size < 1024 * 1024) return `${(size / 1024).toFixed(2)} KB`;
    return `${(size / (1024 * 1024)).toFixed(2)} MB`;
  };

  return (
    <Box>
      <Paper sx={{ p: 3 }}>
        <Typography variant="h6" gutterBottom>
          Firmware Preparation Tool
        </Typography>
        <Typography variant="body2" color="text.secondary" gutterBottom>
          Prepare firmware files for OTA updates with manifest generation
        </Typography>

        <Divider sx={{ my: 3 }} />

        {/* File Upload */}
        <Box sx={{ mb: 3 }}>
          <Typography variant="subtitle2" gutterBottom>
            1. Upload Firmware File
          </Typography>
          <Box sx={{ display: 'flex', gap: 2, alignItems: 'center', mt: 2 }}>
            <Button
              variant="contained"
              component="label"
              startIcon={<UploadIcon />}
            >
              Choose File
              <input
                type="file"
                hidden
                accept=".bin,.hex,.elf"
                onChange={handleFileChange}
              />
            </Button>
            {file && (
              <Box>
                <Chip label={file.name} onDelete={() => setFile(null)} />
                <Typography variant="caption" color="text.secondary" sx={{ ml: 1 }}>
                  {getFileSize()}
                </Typography>
              </Box>
            )}
          </Box>
        </Box>

        {/* Version Input */}
        <Box sx={{ mb: 3 }}>
          <Typography variant="subtitle2" gutterBottom>
            2. Specify Version
          </Typography>
          <TextField
            fullWidth
            label="Firmware Version"
            value={version}
            onChange={(e) => setVersion(e.target.value)}
            placeholder="e.g., 1.0.4"
            helperText="Semantic versioning format (major.minor.patch)"
            sx={{ mt: 1 }}
          />
        </Box>

        {/* Algorithm Selector */}
        <Box sx={{ mb: 3 }}>
          <Typography variant="subtitle2" gutterBottom>
            3. Select Compression Algorithm
          </Typography>
          <FormControl fullWidth sx={{ mt: 1 }}>
            <InputLabel>Algorithm</InputLabel>
            <Select
              value={algorithm}
              label="Algorithm"
              onChange={(e) => setAlgorithm(e.target.value)}
            >
              {algorithms.map((algo) => (
                <MenuItem key={algo.value} value={algo.value}>
                  {algo.label}
                </MenuItem>
              ))}
            </Select>
          </FormControl>
        </Box>

        {/* Action Buttons */}
        <Stack direction="row" spacing={2} sx={{ mb: 3 }}>
          <Button
            variant="contained"
            color="primary"
            onClick={handlePrepare}
            disabled={!file || !version || prepareMutation.isPending}
            startIcon={prepareMutation.isPending ? <CircularProgress size={20} /> : <GenerateIcon />}
          >
            {prepareMutation.isPending ? 'Preparing...' : 'Prepare Firmware'}
          </Button>
          <Button
            variant="outlined"
            onClick={handleClear}
            startIcon={<ClearIcon />}
            disabled={prepareMutation.isPending}
          >
            Clear
          </Button>
        </Stack>

        {/* Error Display */}
        {prepareMutation.isError && (
          <Alert severity="error" sx={{ mb: 2 }}>
            {prepareMutation.error?.response?.data?.error || 'Failed to prepare firmware'}
          </Alert>
        )}

        {/* Success Display */}
        {prepareMutation.isSuccess && manifest && (
          <Alert severity="success" sx={{ mb: 2 }}>
            {manifest.message || 'Firmware prepared successfully!'}
          </Alert>
        )}

        {/* Manifest Display */}
        {manifest && manifest.manifest && (
          <Box>
            <Divider sx={{ my: 3 }} />
            <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', mb: 2 }}>
              <Typography variant="h6">
                Generated Manifest
              </Typography>
              <Button
                variant="outlined"
                size="small"
                startIcon={<DownloadIcon />}
                onClick={downloadManifest}
              >
                Download Manifest
              </Button>
            </Box>
            
            <Paper 
              sx={{ 
                p: 2, 
                bgcolor: 'grey.100', 
                fontFamily: 'monospace',
                fontSize: '0.875rem',
                overflow: 'auto',
                maxHeight: 400
              }}
            >
              <pre style={{ margin: 0 }}>
                {JSON.stringify(manifest.manifest, null, 2)}
              </pre>
            </Paper>

            {/* Files Generated */}
            {manifest.files && manifest.files.length > 0 && (
              <Box sx={{ mt: 2 }}>
                <Typography variant="subtitle2" gutterBottom>
                  Files Generated:
                </Typography>
                <Stack direction="row" spacing={1} flexWrap="wrap">
                  {manifest.files.map((filename, index) => (
                    <Chip key={index} label={filename} size="small" />
                  ))}
                </Stack>
              </Box>
            )}

            {/* Output Log */}
            {manifest.output && (
              <Box sx={{ mt: 2 }}>
                <Typography variant="subtitle2" gutterBottom>
                  Preparation Log:
                </Typography>
                <Paper 
                  sx={{ 
                    p: 2, 
                    bgcolor: 'grey.50',
                    fontSize: '0.75rem',
                    fontFamily: 'monospace',
                    maxHeight: 200,
                    overflow: 'auto'
                  }}
                >
                  <pre style={{ margin: 0 }}>
                    {manifest.output}
                  </pre>
                </Paper>
              </Box>
            )}
          </Box>
        )}

        {/* Instructions */}
        <Box sx={{ mt: 4, p: 2, bgcolor: 'info.lighter', borderRadius: 1 }}>
          <Typography variant="subtitle2" gutterBottom>
            ℹ️ Instructions:
          </Typography>
          <Typography variant="body2" component="div">
            <ul style={{ margin: 0, paddingLeft: '1.5rem' }}>
              <li>Upload a firmware file (.bin, .hex, or .elf format)</li>
              <li>Specify the version number (e.g., 1.0.4)</li>
              <li>Select compression algorithm (ZLIB recommended)</li>
              <li>Click "Prepare Firmware" to generate manifest</li>
              <li>Download the manifest for use in OTA updates</li>
            </ul>
          </Typography>
        </Box>
      </Paper>
    </Box>
  );
};

export default FirmwarePrep;
