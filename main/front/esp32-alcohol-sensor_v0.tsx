import React, { useState, useEffect } from 'react';
import { Trophy, Zap, User, Calendar } from 'lucide-react';

const AlcoholSensorApp = () => {
  const [currentReading, setCurrentReading] = useState(0);
  const [highScores, setHighScores] = useState([
    { id: 1, name: 'AAA', score: 950, date: '2024-01-15' },
    { id: 2, name: 'BBB', score: 890, date: '2024-01-14' },
    { id: 3, name: 'CCC', score: 850, date: '2024-01-13' },
    { id: 4, name: 'DDD', score: 820, date: '2024-01-12' },
    { id: 5, name: 'EEE', score: 780, date: '2024-01-11' },
  ]);
  const [showNameInput, setShowNameInput] = useState(false);
  const [playerName, setPlayerName] = useState('');
  const [pendingScore, setPendingScore] = useState(null);

  // Simulate sensor reading updates
  useEffect(() => {
    const interval = setInterval(() => {
      // Simulate realistic alcohol sensor readings (0-1000 range)
      const newReading = Math.floor(Math.random() * 1000);
      setCurrentReading(newReading);
    }, 2000);

    return () => clearInterval(interval);
  }, []);

  const isTopScore = (score) => {
    if (highScores.length < 10) return true;
    return score > highScores[highScores.length - 1].score;
  };

  const handleRegisterScore = () => {
    if (isTopScore(currentReading)) {
      setPendingScore(currentReading);
      setShowNameInput(true);
    }
  };

  const submitHighScore = () => {
    if (playerName.length === 3 && pendingScore !== null) {
      const newScore = {
        id: Date.now(),
        name: playerName.toUpperCase(),
        score: pendingScore,
        date: new Date().toISOString().split('T')[0]
      };

      const updatedScores = [...highScores, newScore]
        .sort((a, b) => b.score - a.score)
        .slice(0, 10);

      setHighScores(updatedScores);
      setPlayerName('');
      setShowNameInput(false);
      setPendingScore(null);
    }
  };

  const getRankIcon = (index) => {
    if (index === 0) return '🥇';
    if (index === 1) return '🥈';
    if (index === 2) return '🥉';
    return `#${index + 1}`;
  };

  const getScoreColor = (score) => {
    if (score >= 900) return 'text-red-500';
    if (score >= 700) return 'text-orange-500';
    if (score >= 500) return 'text-yellow-500';
    return 'text-green-500';
  };

  return (
    <div className="min-h-screen bg-gradient-to-br from-purple-900 via-blue-900 to-indigo-900 p-4">
      <div className="max-w-4xl mx-auto">
        {/* Header */}
        <div className="text-center mb-8">
          <h1 className="text-4xl font-bold text-white mb-2 flex items-center justify-center gap-3">
            <Zap className="text-yellow-400" />
            ESP32-C3 Alcohol Monitor
            <Zap className="text-yellow-400" />
          </h1>
          <p className="text-blue-200">Real-time alcohol sensor readings with highscore tracking</p>
        </div>

        {/* Current Reading Display */}
        <div className="bg-white/10 backdrop-blur-lg rounded-2xl p-6 mb-8 border border-white/20">
          <div className="text-center">
            <h2 className="text-2xl font-semibold text-white mb-4 flex items-center justify-center gap-2">
              <Zap className="text-yellow-400" />
              Current Reading
            </h2>
            <div className="text-6xl font-bold mb-4">
              <span className={`${getScoreColor(currentReading)}`}>
                {currentReading}
              </span>
              <span className="text-white/60 text-2xl ml-2">ppm</span>
            </div>
            <div className="flex justify-center gap-4">
              <button
                onClick={handleRegisterScore}
                disabled={!isTopScore(currentReading)}
                className={`px-6 py-3 rounded-lg font-semibold transition-all ${
                  isTopScore(currentReading)
                    ? 'bg-gradient-to-r from-green-500 to-blue-500 text-white hover:from-green-600 hover:to-blue-600 shadow-lg'
                    : 'bg-gray-500 text-gray-300 cursor-not-allowed'
                }`}
              >
                {isTopScore(currentReading) ? 'Register Score!' : 'Not Top 10'}
              </button>
            </div>
          </div>
        </div>

        {/* Name Input Modal */}
        {showNameInput && (
          <div className="fixed inset-0 bg-black/50 flex items-center justify-center p-4 z-50">
            <div className="bg-white rounded-2xl p-8 max-w-md w-full">
              <h3 className="text-2xl font-bold text-gray-800 mb-4 text-center">
                New High Score!
              </h3>
              <p className="text-gray-600 mb-6 text-center">
                Score: <span className="font-bold text-2xl text-blue-600">{pendingScore}</span>
              </p>
              <div className="mb-6">
                <label className="block text-gray-700 font-semibold mb-2">
                  Enter your name (3 characters):
                </label>
                <input
                  type="text"
                  value={playerName}
                  onChange={(e) => setPlayerName(e.target.value.slice(0, 3))}
                  className="w-full px-4 py-3 border-2 border-gray-300 rounded-lg focus:border-blue-500 focus:outline-none text-center text-2xl font-bold uppercase"
                  maxLength="3"
                  placeholder="AAA"
                />
              </div>
              <div className="flex gap-4">
                <button
                  onClick={() => {
                    setShowNameInput(false);
                    setPendingScore(null);
                    setPlayerName('');
                  }}
                  className="flex-1 px-4 py-3 bg-gray-500 text-white rounded-lg font-semibold hover:bg-gray-600 transition-colors"
                >
                  Cancel
                </button>
                <button
                  onClick={submitHighScore}
                  disabled={playerName.length !== 3}
                  className={`flex-1 px-4 py-3 rounded-lg font-semibold transition-colors ${
                    playerName.length === 3
                      ? 'bg-blue-500 text-white hover:bg-blue-600'
                      : 'bg-gray-300 text-gray-500 cursor-not-allowed'
                  }`}
                >
                  Submit
                </button>
              </div>
            </div>
          </div>
        )}

        {/* High Scores Table */}
        <div className="bg-white/10 backdrop-blur-lg rounded-2xl p-6 border border-white/20">
          <h2 className="text-2xl font-semibold text-white mb-6 flex items-center justify-center gap-2">
            <Trophy className="text-yellow-400" />
            High Scores
          </h2>
          
          <div className="space-y-3">
            {highScores.map((score, index) => (
              <div
                key={score.id}
                className={`flex items-center justify-between p-4 rounded-lg transition-all ${
                  index < 3 
                    ? 'bg-gradient-to-r from-yellow-500/20 to-orange-500/20 border border-yellow-500/30' 
                    : 'bg-white/5 border border-white/10'
                } hover:bg-white/10`}
              >
                <div className="flex items-center gap-4">
                  <div className="text-2xl font-bold w-12 text-center">
                    {getRankIcon(index)}
                  </div>
                  <div className="flex items-center gap-2">
                    <User className="text-blue-400 w-5 h-5" />
                    <span className="text-white font-bold text-lg">{score.name}</span>
                  </div>
                </div>
                
                <div className="flex items-center gap-6">
                  <div className="text-right">
                    <div className={`text-2xl font-bold ${getScoreColor(score.score)}`}>
                      {score.score}
                    </div>
                    <div className="text-white/60 text-sm">ppm</div>
                  </div>
                  <div className="flex items-center gap-1 text-white/60">
                    <Calendar className="w-4 h-4" />
                    <span className="text-sm">{score.date}</span>
                  </div>
                </div>
              </div>
            ))}
          </div>

          {highScores.length === 0 && (
            <div className="text-center py-12">
              <Trophy className="w-16 h-16 text-gray-400 mx-auto mb-4" />
              <p className="text-gray-400 text-lg">No high scores yet!</p>
              <p className="text-gray-500">Be the first to register a score.</p>
            </div>
          )}
        </div>

        {/* Footer */}
        <div className="text-center mt-8 text-white/60">
          <p>ESP32-C3 Alcohol Sensor Monitor | Real-time readings updated every 2 seconds</p>
        </div>
      </div>
    </div>
  );
};

export default AlcoholSensorApp;