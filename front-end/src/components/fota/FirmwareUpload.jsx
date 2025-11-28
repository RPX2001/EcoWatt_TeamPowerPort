/**
 * FirmwareUpload Component
 * 
 * Handles firmware file upload with drag & drop support
 * Features:
 * - Drag and drop file upload
 * - Browse file selection
 * - Version input
 * - Upload progress bar
 * - Manifest preview
 */

import React, { useState } from 'react';
import {
  Box,
  Paper,
  Typography,
  TextField,
  Button,
  LinearProgress,
  Alert,
  Stack,
  Chip,
  IconButton,
  Divider
} from '@mui/material';
import {
  CloudUpload as UploadIcon,
  CheckCircle as SuccessIcon,
  Error as ErrorIcon,
  Delete as DeleteIcon
} from '@mui/icons-material';
import { useMutation, useQueryClient } from '@tanstack/react-query';
import { uploadFirmware } from '../../api/ota';

const FirmwareUpload = () => {
  const [selectedFile, setSelectedFile] = useState(null);
  const [version, setVersion] = useState('');
  const [uploadProgress, setUploadProgress] = useState(0);
  const [dragActive, setDragActive] = useState(false);
  const queryClient = useQueryClient();

  const uploadMutation = useMutation({
    mutationFn: ({ file, version }) => 
      uploadFirmware(file, version, setUploadProgress),
    onSuccess: (response) => {
      // Invalidate firmware list query
      queryClient.invalidateQueries(['firmwareList']);
      // Reset form
      setTimeout(() => {
        setSelectedFile(null);
        setVersion('');
        setUploadProgress(0);
      }, 2000);
    }
  });

  const handleDrag = (e) => {
    e.preventDefault();
    e.stopPropagation();
    if (e.type === 'dragenter' || e.type === 'dragover') {
      setDragActive(true);
    } else if (e.type === 'dragleave') {
      setDragActive(false);
    }
  };

  const handleDrop = (e) => {
    e.preventDefault();
    e.stopPropagation();
    setDragActive(false);
    
    if (e.dataTransfer.files && e.dataTransfer.files[0]) {
      handleFile(e.dataTransfer.files[0]);
    }
  };

  const handleFileSelect = (e) => {
    if (e.target.files && e.target.files[0]) {
      handleFile(e.target.files[0]);
    }
  };

  const handleFile = (file) => {
    // Validate file
    if (file.name.endsWith('.bin') || file.name.endsWith('.hex') || file.name.endsWith('.elf')) {
      setSelectedFile(file);
    } else {
      alert('Please select a valid firmware file (.bin, .hex, or .elf)');
    }
  };

  const handleRemoveFile = () => {
    setSelectedFile(null);
    setUploadProgress(0);
  };

  const handleUpload = () => {
    if (!selectedFile) {
      alert('Please select a file');
      return;
    }
    if (!version) {
      alert('Please enter a version');
      return;
    }

    uploadMutation.mutate({ file: selectedFile, version });
  };

  const formatFileSize = (bytes) => {
    if (bytes === 0) return '0 Bytes';
    const k = 1024;
    const sizes = ['Bytes', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return Math.round(bytes / Math.pow(k, i) * 100) / 100 + ' ' + sizes[i];
  };

  const isUploading = uploadMutation.isPending;
  const isSuccess = uploadMutation.isSuccess;
  const isError = uploadMutation.isError;

  return (
    <Paper sx={{ p: 3 }}>
      <Typography variant="h6" gutterBottom>
        Upload Firmware
      </Typography>

      {/* Drag & Drop Area */}
      <Box
        onDragEnter={handleDrag}
        onDragLeave={handleDrag}
        onDragOver={handleDrag}
        onDrop={handleDrop}
        sx={{
          border: '2px dashed',
          borderColor: dragActive ? 'primary.main' : 'grey.300',
          borderRadius: 2,
          p: 4,
          textAlign: 'center',
          bgcolor: dragActive ? 'action.hover' : 'background.default',
          cursor: 'pointer',
          transition: 'all 0.3s',
          '&:hover': {
            borderColor: 'primary.main',
            bgcolor: 'action.hover'
          }
        }}
      >
        <input
          type="file"
          id="firmware-file-input"
          accept=".bin,.hex,.elf"
          onChange={handleFileSelect}
          style={{ display: 'none' }}
        />
        <label htmlFor="firmware-file-input" style={{ cursor: 'pointer', display: 'block' }}>
          <UploadIcon sx={{ fontSize: 48, color: 'primary.main', mb: 2 }} />
          <Typography variant="h6" gutterBottom>
            Drag & drop firmware file here
          </Typography>
          <Typography variant="body2" color="text.secondary">
            or click to browse
          </Typography>
          <Typography variant="caption" color="text.secondary" display="block" sx={{ mt: 1 }}>
            Supported formats: .bin, .hex, .elf
          </Typography>
        </label>
      </Box>

      {/* Selected File Info */}
      {selectedFile && (
        <Box sx={{ mt: 3 }}>
          <Alert 
            severity="info"
            action={
              !isUploading && (
                <IconButton
                  size="small"
                  onClick={handleRemoveFile}
                  color="inherit"
                >
                  <DeleteIcon />
                </IconButton>
              )
            }
          >
            <Stack direction="row" spacing={2} alignItems="center">
              <Typography variant="body2" fontWeight="bold">
                {selectedFile.name}
              </Typography>
              <Chip 
                label={formatFileSize(selectedFile.size)} 
                size="small" 
                variant="outlined"
              />
            </Stack>
          </Alert>
        </Box>
      )}

      <Divider sx={{ my: 3 }} />

      {/* Version Input */}
      <TextField
        fullWidth
        label="Firmware Version"
        value={version}
        onChange={(e) => setVersion(e.target.value)}
        placeholder="e.g., 1.0.5"
        disabled={isUploading}
        helperText="Enter the version number for this firmware"
        sx={{ mb: 3 }}
      />

      {/* Upload Progress */}
      {isUploading && (
        <Box sx={{ mb: 3 }}>
          <Typography variant="body2" gutterBottom>
            Uploading... {uploadProgress}%
          </Typography>
          <LinearProgress variant="determinate" value={uploadProgress} />
        </Box>
      )}

      {/* Success Message */}
      {isSuccess && (
        <Alert severity="success" icon={<SuccessIcon />} sx={{ mb: 3 }}>
          Firmware uploaded successfully!
        </Alert>
      )}

      {/* Error Message */}
      {isError && (
        <Alert severity="error" icon={<ErrorIcon />} sx={{ mb: 3 }}>
          Upload failed: {uploadMutation.error?.response?.data?.error || uploadMutation.error?.message || 'Unknown error'}
        </Alert>
      )}

      {/* Upload Button */}
      <Button
        variant="contained"
        fullWidth
        onClick={handleUpload}
        disabled={!selectedFile || !version || isUploading}
        startIcon={<UploadIcon />}
        size="large"
      >
        {isUploading ? `Uploading... ${uploadProgress}%` : 'Upload Firmware'}
      </Button>

      {/* Info Section */}
      <Box sx={{ mt: 3, p: 2, bgcolor: 'info.50', borderRadius: 1 }}>
        <Typography variant="subtitle2" gutterBottom>
          Upload Instructions:
        </Typography>
        <Typography variant="body2" component="ul" sx={{ pl: 2, mb: 0 }}>
          <li>Select a firmware binary file (.bin, .hex, or .elf)</li>
          <li>Enter a unique version number (e.g., 1.0.5)</li>
          <li>Click "Upload Firmware" to upload to the server</li>
          <li>The firmware will be prepared and made available for OTA updates</li>
        </Typography>
      </Box>
    </Paper>
  );
};

export default FirmwareUpload;
