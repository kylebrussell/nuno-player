import React, { useEffect, useState } from 'react';
import { initializeWasm, sendClickWheelEvent, getUIState } from './wasm-bridge';

const Simulator = () => {
  const [uiState, setUiState] = useState(null);
  const [error, setError] = useState(null);
  const [isInitialized, setIsInitialized] = useState(false);

  // Initialize WASM on component mount
  useEffect(() => {
    async function init() {
      try {
        await initializeWasm();
        setIsInitialized(true);
        // Start UI update loop
        updateUIState();
      } catch (err) {
        setError(err.message);
        console.error('Initialization error:', err);
      }
    }
    init();
  }, []);

  // Update UI state periodically
  const updateUIState = () => {
    try {
      const newState = getUIState();
      setUiState(newState);
    } catch (err) {
      console.error('Error updating UI state:', err);
      setError(err.message);
    }
    requestAnimationFrame(updateUIState);
  };

  // Handle click wheel events
  const handleEvent = (event) => {
    try {
      sendClickWheelEvent(event);
    } catch (err) {
      console.error('Error sending event:', err);
      setError(err.message);
    }
  };

  // Keyboard event handler
  useEffect(() => {
    const handleKeyPress = (e) => {
      const keyMap = {
        'ArrowUp': 'UP',
        'ArrowDown': 'DOWN',
        'ArrowLeft': 'LEFT',
        'ArrowRight': 'RIGHT',
        'Enter': 'CENTER',
        ',': 'COUNTERCLOCKWISE',
        '.': 'CLOCKWISE'
      };

      if (keyMap[e.key]) {
        handleEvent(keyMap[e.key]);
        e.preventDefault();
      }
    };

    window.addEventListener('keydown', handleKeyPress);
    return () => window.removeEventListener('keydown', handleKeyPress);
  }, []);

  if (error) {
    return (
      <div className="text-red-600 p-4 border border-red-300 rounded">
        Error: {error}
      </div>
    );
  }

  if (!isInitialized || !uiState) {
    return (
      <div className="text-gray-600 p-4">
        Initializing...
      </div>
    );
  }

  return (
    <div className="p-4">
      <h2 className="text-xl font-bold mb-4">{uiState.menuTitle}</h2>
      
      <div className="mb-4">
        {uiState.menuItems.map((item, index) => (
          <div
            key={index}
            className={`p-2 ${
              index === uiState.selectedItem ? 'bg-blue-100 font-bold' : ''
            }`}
          >
            {item}
          </div>
        ))}
      </div>

      <div className="grid grid-cols-3 gap-2 max-w-xs mx-auto">
        {/* Click wheel controls */}
        <button
          className="p-2 border rounded"
          onClick={() => handleEvent('UP')}
        >
          Up
        </button>
        <button
          className="p-2 border rounded"
          onClick={() => handleEvent('CENTER')}
        >
          Center
        </button>
        <button
          className="p-2 border rounded"
          onClick={() => handleEvent('DOWN')}
        >
          Down
        </button>
        <button
          className="p-2 border rounded"
          onClick={() => handleEvent('LEFT')}
        >
          Left
        </button>
        <div></div> {/* Empty cell for grid layout */}
        <button
          className="p-2 border rounded"
          onClick={() => handleEvent('RIGHT')}
        >
          Right
        </button>
        <button
          className="p-2 border rounded"
          onClick={() => handleEvent('COUNTERCLOCKWISE')}
        >
          CCW
        </button>
        <div></div> {/* Empty cell for grid layout */}
        <button
          className="p-2 border rounded"
          onClick={() => handleEvent('CLOCKWISE')}
        >
          CW
        </button>
      </div>

      <div className="mt-4 text-sm text-gray-600">
        <p>Current Menu: {uiState.currentMenu}</p>
        <p>Scroll Position: {uiState.scrollPosition.toFixed(2)}</p>
        <p>Playing: {uiState.isPlaying ? 'Yes' : 'No'}</p>
      </div>
    </div>
  );
};

export default Simulator;