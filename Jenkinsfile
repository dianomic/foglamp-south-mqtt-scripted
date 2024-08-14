timestamps {
    node("ubuntu18-agent") {
        catchError {
            checkout scm

            dir_exists = sh (
		        script: "test -d 'tests' && echo 'Y' || echo 'N' ",
                returnStdout: true
            ).trim()

            if (dir_exists == 'N'){
                currentBuild.result= 'FAILURE'
                echo "No tests directory found! Exiting."
                return
            }
            try {
                stage("Prerequisite") {
                    // Change to corresponding CORE_BRANCH as required
                    // e.g. FOGL-xxxx, main etc.
                    sh '''
                        CORE_BRANCH=develop
                        ${HOME}/buildFogLAMP ${CORE_BRANCH} ${WORKSPACE}
                    '''
                }
            } catch (e) {
                currentBuild.result = 'SUCCESS'
                echo "Failed to build FogLAMP; required to run the tests!"
                return
            }

            try {
                stage("Run Tests") {
                    echo "Executing tests..."
                    sh '''
                        export FOGLAMP_ROOT=${WORKSPACE}/FogLAMP && export PYTHONPATH=${WORKSPACE}/FogLAMP:
                        cd tests && cmake . && make && ./RunTests --gtest_output=xml:test_output.xml
                    '''
                    echo "Done."
                }
            } catch (e) {
                result = "TESTS FAILED"
                currentBuild.result = 'FAILURE'
                echo "Tests failed!"
            }

            try {
                stage("Publish Test Report") {
                    junit "tests/test_output.xml"
                }
            } catch (e) {
                result = "TEST REPORT GENERATION FAILED"
                currentBuild.result = 'FAILURE'
                echo "Failed to generate test reports!"
            }
        }
        stage ("Cleanup"){
            // Add here if any cleanup is required
            echo "Done."
        }
    }
}
